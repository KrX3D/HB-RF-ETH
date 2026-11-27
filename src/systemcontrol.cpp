/* 
 *  systemcontrol.cpp is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
 *  
 *  Copyright 2026 HB-RF-ETH contributors
 *  
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *  
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *  
 */

#include "systemcontrol.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "SystemControl";

#define RTC_MAGIC 0x48425246 // "HBRF"

// Survives a software restart, but not a power loss
typedef struct
{
    uint32_t magic;
    char restartReason[24];
    int networkWatchdogRestarts;
} rtc_state_t;

static RTC_NOINIT_ATTR rtc_state_t _rtcState;
static char _resetReason[24] = "unknown";

static void ensureRtcState()
{
    if (_rtcState.magic != RTC_MAGIC)
    {
        memset(&_rtcState, 0, sizeof(_rtcState));
        _rtcState.magic = RTC_MAGIC;
    }
}

void SystemControl::init()
{
    esp_reset_reason_t reason = esp_reset_reason();
    bool rtcValid = _rtcState.magic == RTC_MAGIC;

    switch (reason)
    {
    case ESP_RST_POWERON:
        strcpy(_resetReason, "power_on");
        break;
    case ESP_RST_BROWNOUT:
        strcpy(_resetReason, "brownout");
        break;
    case ESP_RST_TASK_WDT:
        strcpy(_resetReason, "task_watchdog");
        break;
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT:
        strcpy(_resetReason, "watchdog");
        break;
    case ESP_RST_PANIC:
        strcpy(_resetReason, "panic");
        break;
    case ESP_RST_SW:
        if (rtcValid && _rtcState.restartReason[0])
        {
            strncpy(_resetReason, _rtcState.restartReason, sizeof(_resetReason) - 1);
        }
        else
        {
            strcpy(_resetReason, "software");
        }
        break;
    default:
        strcpy(_resetReason, "other");
        break;
    }

    if (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT || !rtcValid)
    {
        memset(&_rtcState, 0, sizeof(_rtcState));
        _rtcState.magic = RTC_MAGIC;
    }
    _rtcState.restartReason[0] = 0;

    ESP_LOGI(TAG, "Reset reason: %s", _resetReason);
}

const char *SystemControl::getResetReason()
{
    return _resetReason;
}

int SystemControl::getNetworkWatchdogRestarts()
{
    ensureRtcState();
    return _rtcState.networkWatchdogRestarts;
}

void SystemControl::setNetworkWatchdogRestarts(int count)
{
    ensureRtcState();
    _rtcState.networkWatchdogRestarts = count;
}

static void restartTask(void *parameter)
{
    vTaskDelay((int)(intptr_t)parameter / portTICK_PERIOD_MS);
    esp_restart();
}

void SystemControl::restart(const char *reason, int delayMs)
{
    static bool pending = false;
    if (pending)
        return;
    pending = true;

    ensureRtcState();
    strncpy(_rtcState.restartReason, reason, sizeof(_rtcState.restartReason) - 1);
    _rtcState.restartReason[sizeof(_rtcState.restartReason) - 1] = 0;

    ESP_LOGW(TAG, "Restarting in %d ms, reason: %s", delayMs, reason);
    xTaskCreate(restartTask, "Restart", 2048, (void *)(intptr_t)delayMs, 20, NULL);
}

void SystemControl::confirmFirmware(bool (*isNetworkUp)())
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY)
    {
        esp_ota_mark_app_valid_cancel_rollback();
        return;
    }

    // first start of a freshly installed firmware: keep it only if the network comes up
    ESP_LOGW(TAG, "New firmware, waiting up to 3 minutes for the network before confirming it");
    for (int i = 0; i < 180; i++)
    {
        if (isNetworkUp())
        {
            ESP_LOGI(TAG, "Network is up, new firmware confirmed");
            esp_ota_mark_app_valid_cancel_rollback();
            return;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "Network did not come up, rolling back to the previous firmware");
    ensureRtcState();
    strcpy(_rtcState.restartReason, "rollback");
    esp_ota_mark_app_invalid_rollback_and_reboot();
}
