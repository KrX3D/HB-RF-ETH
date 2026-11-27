/* 
 *  networkwatchdog.h is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"
#include "ethernet.h"

// Restarts the board when the network stays unreachable (no link, no IP address or
// the gateway does not answer pings) for the configured number of minutes.
// Repeated restarts back off exponentially (timeout, 2x, 4x, ... up to 24 h), so a
// network that is down for real does not cause a restart every few minutes.
class NetworkWatchdog
{
private:
    Settings *_settings;
    Ethernet *_ethernet;
    TaskHandle_t _tHandle = NULL;
    bool pingGateway(ip4_addr_t gateway);

public:
    NetworkWatchdog(Settings *settings, Ethernet *ethernet);
    void start();
    void stop();
    void _taskFunc();
};
