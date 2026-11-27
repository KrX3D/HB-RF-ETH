/* 
 *  mqttpublisher.cpp is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

#include "mqttpublisher.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "led.h"
#include "systemcontrol.h"

static const char *TAG = "MQTT";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ((MqttPublisher *)handler_args)->_handleEvent((esp_mqtt_event_handle_t)event_data);
}

static void mqtt_task_func(void *parameter)
{
    ((MqttPublisher *)parameter)->_taskFunc();
}

MqttPublisher::MqttPublisher(Settings *settings, DeviceStatus *deviceStatus, UpdateCheck *updateCheck)
    : _settings(settings), _deviceStatus(deviceStatus), _updateCheck(updateCheck), _connected(false), _publishAll(false), _publishStateNow(false)
{
}

void MqttPublisher::start()
{
    if (!_settings->getMqttEnabled())
    {
        ESP_LOGI(TAG, "MQTT is disabled");
        return;
    }

    if (strlen(_settings->getMqttServer()) == 0)
    {
        ESP_LOGW(TAG, "MQTT is enabled, but no broker is configured");
        return;
    }

    const char *prefix = _settings->getMqttTopicPrefix();
    if (strlen(prefix) > 0)
    {
        strncpy(_topicPrefix, prefix, sizeof(_topicPrefix) - 1);
    }
    else
    {
        snprintf(_topicPrefix, sizeof(_topicPrefix), "hb-rf-eth/%s", _deviceStatus->getSerialNumber());
    }
    size_t len = strlen(_topicPrefix);
    while (len > 0 && _topicPrefix[len - 1] == '/')
    {
        _topicPrefix[--len] = 0;
    }

    snprintf(_statusTopic, sizeof(_statusTopic), "%s/status", _topicPrefix);
    snprintf(_stateTopic, sizeof(_stateTopic), "%s/state", _topicPrefix);
    snprintf(_updateTopic, sizeof(_updateTopic), "%s/update", _topicPrefix);
    snprintf(_commandTopic, sizeof(_commandTopic), "%s/command", _topicPrefix);
    snprintf(_deviceId, sizeof(_deviceId), "hb-rf-eth_%s", _deviceStatus->getSerialNumber());

    esp_mqtt_client_config_t config = {};
    config.host = _settings->getMqttServer();
    config.port = _settings->getMqttPort();
    config.transport = MQTT_TRANSPORT_OVER_TCP;
    config.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    config.client_id = _deviceId;
    if (strlen(_settings->getMqttUser()) > 0)
    {
        config.username = _settings->getMqttUser();
        config.password = _settings->getMqttPassword();
    }
    config.keepalive = 60;
    config.lwt_topic = _statusTopic;
    config.lwt_msg = "offline";
    config.lwt_qos = 1;
    config.lwt_retain = 1;
    config.buffer_size = 2048;

    _client = esp_mqtt_client_init(&config);
    if (_client == NULL)
    {
        ESP_LOGE(TAG, "Could not initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, mqtt_event_handler, this);

    if (esp_mqtt_client_start(_client) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not start MQTT client");
        esp_mqtt_client_destroy(_client);
        _client = NULL;
        return;
    }

    xTaskCreate(mqtt_task_func, "MQTT_Publisher", 4096, this, 3, &_tHandle);

    ESP_LOGI(TAG, "MQTT client started, broker %s:%d, topic prefix %s", _settings->getMqttServer(), _settings->getMqttPort(), _topicPrefix);
}

void MqttPublisher::stop()
{
    if (_tHandle != NULL)
    {
        vTaskDelete(_tHandle);
        _tHandle = NULL;
    }

    if (_client != NULL)
    {
        if (_connected)
        {
            esp_mqtt_client_publish(_client, _statusTopic, "offline", 0, 1, 1);
        }
        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client = NULL;
    }

    _connected = false;
}

void MqttPublisher::_handleEvent(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        _connected = true;
        _publishAll = true;
        if (_settings->getMqttCommands())
        {
            char topic[112];
            snprintf(topic, sizeof(topic), "%s/#", _commandTopic);
            esp_mqtt_client_subscribe(_client, topic, 1);
        }
        if (_tHandle != NULL)
        {
            xTaskNotifyGive(_tHandle);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        _connected = false;
        break;

    case MQTT_EVENT_DATA:
    {
        // only complete, small messages below <prefix>/command/ are of interest. Retained
        // messages are ignored, a retained "restart" would otherwise restart the board forever.
        size_t prefixLen = strlen(_commandTopic);
        if (!_settings->getMqttCommands() || event->retain || event->current_data_offset != 0 || event->data_len != event->total_data_len ||
            event->topic_len <= (int)prefixLen + 1 || event->topic_len >= 128 || event->data_len >= 64 ||
            strncmp(event->topic, _commandTopic, prefixLen) != 0 || event->topic[prefixLen] != '/')
            break;

        char command[128];
        char payload[64];
        int commandLen = event->topic_len - prefixLen - 1;
        memcpy(command, event->topic + prefixLen + 1, commandLen);
        command[commandLen] = 0;
        memcpy(payload, event->data, event->data_len);
        payload[event->data_len] = 0;

        handleCommand(command, payload);
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

void MqttPublisher::_taskFunc()
{
    const int64_t intervalUs = (int64_t)_settings->getMqttInterval() * 1000000LL;
    int64_t lastPublish = 0;
    uint32_t lastCcuAddress = 0;
    bool lastCcuRestartRequired = false;
    char lastUpdate[48] = {0};
    int64_t lastUpdatePublish = 0;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, 1000 / portTICK_PERIOD_MS);

        if (!_connected)
            continue;

        int64_t now = esp_timer_get_time();
        uint32_t ccuAddress = _deviceStatus->getCcuAddress();
        bool ccuRestartRequired = _deviceStatus->isCcuRestartRequired();

        // state, latest version and progress (rounded to 5 %, at most every 2 s) of the update
        char update[48];
        snprintf(update, sizeof(update), "%d|%s|%d", (int)_updateCheck->getState(), _updateCheck->getLatestVersion(), _updateCheck->getProgress() / 5);

        if (_publishAll.exchange(false))
        {
            esp_mqtt_client_publish(_client, _statusTopic, "online", 0, 1, 1);
            if (_settings->getMqttHaDiscovery())
            {
                publishDiscovery();
            }
            lastPublish = 0;
            lastUpdate[0] = 0;
        }

        if (lastPublish == 0 || ccuAddress != lastCcuAddress || ccuRestartRequired != lastCcuRestartRequired ||
            _publishStateNow.exchange(false) || (now - lastPublish) >= intervalUs)
        {
            publishState();
            lastPublish = now;
            lastCcuAddress = ccuAddress;
            lastCcuRestartRequired = ccuRestartRequired;
        }

        if (strcmp(update, lastUpdate) != 0 && (now - lastUpdatePublish) >= 2000000)
        {
            publishUpdate();
            strcpy(lastUpdate, update);
            lastUpdatePublish = now;
            publishState();
        }
    }
}

void MqttPublisher::publishState()
{
    cJSON *root = _deviceStatus->toJson();
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json != NULL)
    {
        esp_mqtt_client_publish(_client, _stateTopic, json, 0, 0, 1);
        free(json);
    }
}

cJSON *MqttPublisher::createDiscoveryEntity(const char *key, const char *name, const char *valueTemplate, const char *unit, const char *deviceClass, const char *stateClass, const char *category, const char *icon)
{
    char buf[128];
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", name);
    snprintf(buf, sizeof(buf), "%s_%s", _deviceId, key);
    cJSON_AddStringToObject(root, "unique_id", buf);
    cJSON_AddStringToObject(root, "object_id", buf);
    if (valueTemplate)
    {
        cJSON_AddStringToObject(root, "state_topic", _stateTopic);
        cJSON_AddStringToObject(root, "value_template", valueTemplate);
    }
    cJSON_AddStringToObject(root, "availability_topic", _statusTopic);
    if (unit)
        cJSON_AddStringToObject(root, "unit_of_measurement", unit);
    if (deviceClass)
        cJSON_AddStringToObject(root, "device_class", deviceClass);
    if (stateClass)
        cJSON_AddStringToObject(root, "state_class", stateClass);
    if (category)
        cJSON_AddStringToObject(root, "entity_category", category);
    if (icon)
        cJSON_AddStringToObject(root, "icon", icon);

    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON *identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(_deviceId));
    cJSON_AddStringToObject(device, "name", _deviceStatus->getHostname());
    cJSON_AddStringToObject(device, "model", "HB-RF-ETH");
    cJSON_AddStringToObject(device, "sw_version", _deviceStatus->getFirmwareVersion());

    return root;
}

// Publishes root (and frees it), root == NULL removes the entity from Home Assistant
void MqttPublisher::publishDiscoveryEntity(const char *component, const char *key, cJSON *root)
{
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config", component, _deviceId, key);

    if (root == NULL)
    {
        esp_mqtt_client_publish(_client, topic, "", 0, 1, 1);
        return;
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json != NULL)
    {
        esp_mqtt_client_publish(_client, topic, json, 0, 1, 1);
        free(json);
    }
}

void MqttPublisher::publishDiscoveryEntity(const char *component, const char *key, const char *name, const char *valueTemplate, const char *unit, const char *deviceClass, const char *stateClass, bool diagnostic, const char *icon)
{
    publishDiscoveryEntity(component, key, createDiscoveryEntity(key, name, valueTemplate, unit, deviceClass, stateClass, diagnostic ? "diagnostic" : NULL, icon));
}

void MqttPublisher::publishCommandEntities(bool enabled)
{
    char topic[128];

    if (!enabled)
    {
        publishDiscoveryEntity("button", "restart", NULL);
        publishDiscoveryEntity("button", "update_check", NULL);
        publishDiscoveryEntity("number", "led_brightness", NULL);
        publishDiscoveryEntity("switch", "network_watchdog", NULL);
        publishDiscoveryEntity("update", "firmware", NULL);
        return;
    }

    cJSON *e = createDiscoveryEntity("restart", "Restart", NULL, NULL, "restart", NULL, "config", NULL);
    snprintf(topic, sizeof(topic), "%s/restart", _commandTopic);
    cJSON_AddStringToObject(e, "command_topic", topic);
    publishDiscoveryEntity("button", "restart", e);

    e = createDiscoveryEntity("update_check", "Check for firmware update", NULL, NULL, NULL, NULL, "config", "mdi:update");
    snprintf(topic, sizeof(topic), "%s/update", _commandTopic);
    cJSON_AddStringToObject(e, "command_topic", topic);
    cJSON_AddStringToObject(e, "payload_press", "check");
    publishDiscoveryEntity("button", "update_check", e);

    e = createDiscoveryEntity("led_brightness", "LED brightness", "{{ value_json.settings.ledBrightness }}", "%", NULL, NULL, "config", "mdi:brightness-6");
    snprintf(topic, sizeof(topic), "%s/led_brightness", _commandTopic);
    cJSON_AddStringToObject(e, "command_topic", topic);
    cJSON_AddNumberToObject(e, "min", 0);
    cJSON_AddNumberToObject(e, "max", 100);
    cJSON_AddNumberToObject(e, "step", 5);
    cJSON_AddStringToObject(e, "mode", "slider");
    publishDiscoveryEntity("number", "led_brightness", e);

    e = createDiscoveryEntity("network_watchdog", "Network watchdog", "{{ 'ON' if value_json.settings.networkWatchdog else 'OFF' }}", NULL, NULL, NULL, "config", "mdi:restart-alert");
    snprintf(topic, sizeof(topic), "%s/network_watchdog", _commandTopic);
    cJSON_AddStringToObject(e, "command_topic", topic);
    publishDiscoveryEntity("switch", "network_watchdog", e);

    e = createDiscoveryEntity("firmware", "Firmware", NULL, NULL, "firmware", NULL, "config", NULL);
    cJSON_AddStringToObject(e, "state_topic", _updateTopic);
    snprintf(topic, sizeof(topic), "%s/update", _commandTopic);
    cJSON_AddStringToObject(e, "command_topic", topic);
    cJSON_AddStringToObject(e, "payload_install", "install");
    publishDiscoveryEntity("update", "firmware", e);
}

void MqttPublisher::publishUpdate()
{
    cJSON *status = cJSON_CreateObject();
    _updateCheck->addToJson(status);
    cJSON *u = cJSON_GetObjectItem(status, "update");

    const char *installed = cJSON_GetStringValue(cJSON_GetObjectItem(u, "installedVersion"));
    const char *latest = cJSON_GetStringValue(cJSON_GetObjectItem(u, "latestVersion"));
    bool installing = _updateCheck->getState() == UPDATE_STATE_INSTALLING;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "installed_version", installed);
    // unknown latest version: report the installed one, so HA shows "up to date" instead of "unknown"
    cJSON_AddStringToObject(root, "latest_version", (latest && strcmp(latest, "n/a") != 0) ? latest : installed);
    cJSON_AddStringToObject(root, "title", "HB-RF-ETH Firmware");
    const char *releaseUrl = cJSON_GetStringValue(cJSON_GetObjectItem(u, "releaseUrl"));
    if (releaseUrl && *releaseUrl)
        cJSON_AddStringToObject(root, "release_url", releaseUrl);
    cJSON_AddBoolToObject(root, "in_progress", installing);
    if (installing)
        cJSON_AddNumberToObject(root, "update_percentage", _updateCheck->getProgress());
    else
        cJSON_AddNullToObject(root, "update_percentage");

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    cJSON_Delete(status);

    if (json != NULL)
    {
        esp_mqtt_client_publish(_client, _updateTopic, json, 0, 1, 1);
        free(json);
    }
}

void MqttPublisher::handleCommand(const char *command, const char *payload)
{
    ESP_LOGI(TAG, "Command %s: %s", command, payload);

    if (strcmp(command, "restart") == 0)
    {
        SystemControl::restart("mqtt");
    }
    else if (strcmp(command, "led_brightness") == 0)
    {
        char *end;
        long value = strtol(payload, &end, 10);
        if (end == payload || value < 0 || value > 100)
        {
            ESP_LOGW(TAG, "Invalid LED brightness: %s", payload);
            return;
        }
        _settings->setLEDBrightness((int)value);
        _settings->save();
        LED::setBrightness((int)value);
        _publishStateNow = true;
    }
    else if (strcmp(command, "network_watchdog") == 0)
    {
        bool enabled = strcasecmp(payload, "ON") == 0 || strcmp(payload, "1") == 0 || strcasecmp(payload, "true") == 0;
        _settings->setNetworkWatchdog(enabled, _settings->getNetworkWatchdogTimeout());
        _settings->save();
        _publishStateNow = true;
    }
    else if (strcmp(command, "update") == 0)
    {
        if (strcasecmp(payload, "check") == 0)
            _updateCheck->requestCheck();
        else if (strcasecmp(payload, "install") == 0 && !_updateCheck->requestInstall(false))
            ESP_LOGW(TAG, "No newer firmware to install");
    }
    else
    {
        ESP_LOGW(TAG, "Unknown command %s", command);
    }

    if (_tHandle != NULL)
        xTaskNotifyGive(_tHandle);
}

void MqttPublisher::publishDiscovery()
{
    publishDiscoveryEntity("binary_sensor", "ccu_connected", "CCU connected", "{{ 'ON' if value_json.ccu.connected else 'OFF' }}", NULL, "connectivity", NULL, false, NULL);
    publishDiscoveryEntity("binary_sensor", "ccu_restart_required", "CCU restart required", "{{ 'ON' if value_json.ccu.restartRequired else 'OFF' }}", NULL, "problem", NULL, false, NULL);
    publishDiscoveryEntity("sensor", "reset_reason", "Last restart reason", "{{ value_json.device.resetReason }}", NULL, NULL, NULL, true, "mdi:restart");
    publishDiscoveryEntity("sensor", "update_state", "Firmware update state", "{{ value_json.update.state }}", NULL, NULL, NULL, true, "mdi:update");
    publishCommandEntities(_settings->getMqttCommands());
    publishDiscoveryEntity("sensor", "ccu_address", "CCU address", "{{ value_json.ccu.address }}", NULL, NULL, NULL, false, "mdi:ip-network");
    publishDiscoveryEntity("sensor", "cpu_usage", "CPU usage", "{{ value_json.device.cpuUsage }}", "%", NULL, "measurement", true, "mdi:cpu-32-bit");
    publishDiscoveryEntity("sensor", "memory_usage", "Memory usage", "{{ value_json.device.memoryUsage }}", "%", NULL, "measurement", true, "mdi:memory");
    publishDiscoveryEntity("sensor", "uptime", "Uptime", "{{ value_json.device.uptime }}", "s", "duration", NULL, true, NULL);
    publishDiscoveryEntity("sensor", "ip_address", "IP address", "{{ value_json.device.ipAddress }}", NULL, NULL, NULL, true, "mdi:ip");
    publishDiscoveryEntity("sensor", "firmware_version", "Firmware version", "{{ value_json.device.firmwareVersion }}", NULL, NULL, NULL, true, "mdi:chip");
    publishDiscoveryEntity("sensor", "latest_firmware_version", "Latest firmware version", "{{ value_json.device.latestFirmwareVersion }}", NULL, NULL, NULL, true, "mdi:update");
    publishDiscoveryEntity("binary_sensor", "radio_module_detected", "Radio module detected", "{{ 'ON' if value_json.radioModule.detected else 'OFF' }}", NULL, NULL, NULL, true, "mdi:radio-tower");
    publishDiscoveryEntity("sensor", "radio_module_type", "Radio module type", "{{ value_json.radioModule.type }}", NULL, NULL, NULL, true, "mdi:radio-tower");
    publishDiscoveryEntity("sensor", "radio_module_serial", "Radio module serial", "{{ value_json.radioModule.serial }}", NULL, NULL, NULL, true, "mdi:barcode");
    publishDiscoveryEntity("sensor", "radio_module_firmware", "Radio module firmware", "{{ value_json.radioModule.firmwareVersion }}", NULL, NULL, NULL, true, "mdi:chip");
    publishDiscoveryEntity("sensor", "radio_module_bidcos_mac", "Radio address (BidCos)", "{{ value_json.radioModule.bidCosRadioMAC }}", NULL, NULL, NULL, true, "mdi:identifier");
    publishDiscoveryEntity("sensor", "radio_module_hmip_mac", "Radio address (HmIP)", "{{ value_json.radioModule.hmIPRadioMAC }}", NULL, NULL, NULL, true, "mdi:identifier");
    publishDiscoveryEntity("sensor", "radio_module_sgtin", "SGTIN", "{{ value_json.radioModule.sgtin }}", NULL, NULL, NULL, true, "mdi:barcode");
}
