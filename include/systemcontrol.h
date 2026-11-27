/* 
 *  systemcontrol.h is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

#pragma once

// Restart handling shared by the WebUI, MQTT, the update and the watchdogs.
class SystemControl
{
public:
    // Restarts the board after delayMs, so a pending HTTP or MQTT reply can still go out.
    // reason is kept across the restart and reported by getResetReason() afterwards.
    static void restart(const char *reason, int delayMs = 1500);

    // Why the board started: "power_on", "brownout", "task_watchdog", "panic", or the
    // reason given to restart() ("webui", "mqtt", "update", "network_watchdog", ...)
    static const char *getResetReason();

    // Consecutive restarts by the network watchdog without the network becoming healthy in between
    static int getNetworkWatchdogRestarts();
    static void setNetworkWatchdogRestarts(int count);

    // Call once at startup: remembers the reset reason
    static void init();

    // After an OTA update the new firmware is only kept once the network came up.
    // Otherwise the bootloader switches back to the previous firmware.
    static void confirmFirmware(bool (*isNetworkUp)());
};
