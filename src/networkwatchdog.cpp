/* 
 *  networkwatchdog.cpp is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

#include "networkwatchdog.h"
#include "systemcontrol.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ping/ping_sock.h"
#include "freertos/semphr.h"

static const char *TAG = "NetworkWatchdog";

#define CHECK_INTERVAL_MS 10000
#define PING_INTERVAL_US (30 * 1000000LL)
#define HEALTHY_RESET_US (10 * 60 * 1000000LL)
#define MAX_BACKOFF_MINUTES (24 * 60)

static void network_watchdog_task_func(void *parameter)
{
    ((NetworkWatchdog *)parameter)->_taskFunc();
}

NetworkWatchdog::NetworkWatchdog(Settings *settings, Ethernet *ethernet) : _settings(settings), _ethernet(ethernet)
{
}

void NetworkWatchdog::start()
{
    xTaskCreate(network_watchdog_task_func, "NetworkWatchdog", 3072, this, 3, &_tHandle);
}

void NetworkWatchdog::stop()
{
    if (_tHandle)
    {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }
}

typedef struct
{
    SemaphoreHandle_t done;
    uint32_t received;
} ping_result_t;

static void ping_on_success(esp_ping_handle_t hdl, void *args)
{
    ((ping_result_t *)args)->received++;
}

static void ping_on_end(esp_ping_handle_t hdl, void *args)
{
    xSemaphoreGive(((ping_result_t *)args)->done);
}

bool NetworkWatchdog::pingGateway(ip4_addr_t gateway)
{
    ping_result_t result = {.done = xSemaphoreCreateBinary(), .received = 0};
    if (result.done == NULL)
        return true; // do not restart because of an internal error

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr.type = IPADDR_TYPE_V4;
    config.target_addr.u_addr.ip4 = gateway;
    config.count = 3;
    config.interval_ms = 500;
    config.timeout_ms = 1000;

    esp_ping_callbacks_t callbacks = {};
    callbacks.cb_args = &result;
    callbacks.on_ping_success = ping_on_success;
    callbacks.on_ping_end = ping_on_end;

    esp_ping_handle_t ping;
    if (esp_ping_new_session(&config, &callbacks, &ping) != ESP_OK)
    {
        vSemaphoreDelete(result.done);
        return true;
    }

    esp_ping_start(ping);
    bool finished = xSemaphoreTake(result.done, 10000 / portTICK_PERIOD_MS) == pdTRUE;
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    vSemaphoreDelete(result.done);

    return !finished || result.received > 0;
}

void NetworkWatchdog::_taskFunc()
{
    int64_t unhealthySince = 0;
    int64_t healthySince = esp_timer_get_time();
    int64_t lastPing = 0;
    bool gatewayReachable = true;

    for (;;)
    {
        vTaskDelay(CHECK_INTERVAL_MS / portTICK_PERIOD_MS);

        int64_t now = esp_timer_get_time();

        if (!_settings->getNetworkWatchdogEnabled())
        {
            unhealthySince = 0;
            continue;
        }

        ip4_addr_t ip, netmask, gateway, dns1, dns2;
        _ethernet->getNetworkSettings(&ip, &netmask, &gateway, &dns1, &dns2);

        bool healthy = _ethernet->isConnected() && ip.addr != IPADDR_ANY;

        if (healthy && gateway.addr != IPADDR_ANY)
        {
            // ping more often while something is wrong, to notice the recovery quickly
            if (lastPing == 0 || !gatewayReachable || (now - lastPing) >= PING_INTERVAL_US)
            {
                gatewayReachable = pingGateway(gateway);
                lastPing = now;
            }
            healthy = gatewayReachable;
        }

        if (healthy)
        {
            if (unhealthySince != 0)
            {
                ESP_LOGI(TAG, "Network is reachable again");
                unhealthySince = 0;
                healthySince = now;
            }
            if (SystemControl::getNetworkWatchdogRestarts() > 0 && (now - healthySince) >= HEALTHY_RESET_US)
            {
                SystemControl::setNetworkWatchdogRestarts(0);
            }
            continue;
        }

        if (unhealthySince == 0)
        {
            unhealthySince = now;
            ESP_LOGW(TAG, "Network is unreachable (link/IP %s, gateway %s)", _ethernet->isConnected() ? "ok" : "down", gatewayReachable ? "ok" : "no reply");
        }

        int restarts = SystemControl::getNetworkWatchdogRestarts();
        int64_t limitMinutes = _settings->getNetworkWatchdogTimeout();
        for (int i = 0; i < restarts && limitMinutes < MAX_BACKOFF_MINUTES; i++)
        {
            limitMinutes *= 2;
        }
        if (limitMinutes > MAX_BACKOFF_MINUTES)
            limitMinutes = MAX_BACKOFF_MINUTES;

        if ((now - unhealthySince) >= limitMinutes * 60 * 1000000LL)
        {
            ESP_LOGE(TAG, "Network unreachable for %lld minutes, restarting", limitMinutes);
            SystemControl::setNetworkWatchdogRestarts(restarts + 1);
            SystemControl::restart("network_watchdog", 100);
            vTaskDelay(portMAX_DELAY);
        }
    }

    vTaskDelete(NULL);
}
