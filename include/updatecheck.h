/* 
 *  updatecheck.h is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
 *  
 *  Copyright 2022 Alexander Reinert
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

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "sysinfo.h"
#include "led.h"
#include "settings.h"
#include "updatehelpers.h"

typedef enum
{
    UPDATE_STATE_IDLE = 0,
    UPDATE_STATE_CHECKING,
    UPDATE_STATE_UP_TO_DATE,
    UPDATE_STATE_AVAILABLE,
    UPDATE_STATE_ERROR,
    UPDATE_STATE_INSTALLING,
    UPDATE_STATE_INSTALLED,
} update_state_t;

// Checks the latest release of a GitHub repository (Settings::getUpdateRepo) and
// installs its firmware .bin on request: download over HTTPS, optional SHA-256 check,
// write to the inactive OTA slot, restart. If the new firmware does not come up,
// the bootloader switches back (see SystemControl::confirmFirmware).
class UpdateCheck
{
private:
    Settings *_settings;
    SysInfo *_sysInfo;
    LED *_statusLED;
    TaskHandle_t _tHandle = NULL;
    SemaphoreHandle_t _lock;

    update_state_t _state = UPDATE_STATE_IDLE;
    int _progress = 0;
    char _error[64] = {0};
    char _latestVersion[33] = "n/a";
    release_info_t _release = {};
    bool _hasRelease = false;

    void setState(update_state_t state, const char *error = NULL);
    void check();
    void reportToAuthor();
    void install(bool force);
    bool download(const char *url, const char *sha256, int expectedSize);

public:
    UpdateCheck(Settings *settings, SysInfo *sysInfo, LED *statusLED);
    void start();
    void stop();

    const char *getLatestVersion();
    update_state_t getState();
    int getProgress();
    bool isUpdateAvailable();

    // Triggers a check in the background
    void requestCheck();
    // Installs the latest release in the background. Without force only a newer version is installed.
    // Returns false if no release is known or an installation is already running.
    bool requestInstall(bool force);

    // Adds an "update" object with state, versions, progress and error to parent
    void addToJson(cJSON *parent);
    static const char *stateToString(update_state_t state);

    void _taskFunc();
};
