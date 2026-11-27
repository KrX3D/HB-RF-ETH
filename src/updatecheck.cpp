/* 
 *  updatecheck.cpp is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

#include "updatecheck.h"
#include "systemcontrol.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <strings.h>
#include <limits.h>

static const char *TAG = "UpdateCheck";

// root CAs of GitHub and its download hosts, see certs/github_ca.pem
extern const char github_ca_pem[] asm("_binary_github_ca_pem_start");

#define NOTIFY_CHECK 1
#define NOTIFY_INSTALL 2
#define NOTIFY_INSTALL_FORCE 4

#define MAX_RELEASE_JSON (32 * 1024)

static void _update_check_task_func(void *parameter)
{
    ((UpdateCheck *)parameter)->_taskFunc();
}

UpdateCheck::UpdateCheck(Settings *settings, SysInfo *sysInfo, LED *statusLED) : _settings(settings), _sysInfo(sysInfo), _statusLED(statusLED)
{
    _lock = xSemaphoreCreateMutex();
}

void UpdateCheck::start()
{
    // TLS needs a large stack
    xTaskCreate(_update_check_task_func, "UpdateCheck", 8192, this, 3, &_tHandle);
}

void UpdateCheck::stop()
{
    vTaskDelete(_tHandle);
}

const char *UpdateCheck::stateToString(update_state_t state)
{
    switch (state)
    {
    case UPDATE_STATE_CHECKING:
        return "checking";
    case UPDATE_STATE_UP_TO_DATE:
        return "up_to_date";
    case UPDATE_STATE_AVAILABLE:
        return "available";
    case UPDATE_STATE_ERROR:
        return "error";
    case UPDATE_STATE_INSTALLING:
        return "installing";
    case UPDATE_STATE_INSTALLED:
        return "installed";
    default:
        return "idle";
    }
}

const char *UpdateCheck::getLatestVersion()
{
    return _latestVersion;
}

update_state_t UpdateCheck::getState()
{
    return _state;
}

int UpdateCheck::getProgress()
{
    return _progress;
}

bool UpdateCheck::isUpdateAvailable()
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool res = _hasRelease && compareVersions(_release.version, _sysInfo->getCurrentVersion()) > 0;
    xSemaphoreGive(_lock);
    return res;
}

void UpdateCheck::requestCheck()
{
    xTaskNotify(_tHandle, NOTIFY_CHECK, eSetBits);
}

bool UpdateCheck::requestInstall(bool force)
{
    if (_state == UPDATE_STATE_INSTALLING || _state == UPDATE_STATE_INSTALLED || !_hasRelease)
        return false;
    if (!force && !isUpdateAvailable())
        return false;

    xTaskNotify(_tHandle, force ? NOTIFY_INSTALL_FORCE : NOTIFY_INSTALL, eSetBits);
    return true;
}

void UpdateCheck::addToJson(cJSON *parent)
{
    cJSON *update = cJSON_AddObjectToObject(parent, "update");

    xSemaphoreTake(_lock, portMAX_DELAY);
    cJSON_AddStringToObject(update, "state", stateToString(_state));
    cJSON_AddStringToObject(update, "repo", _settings->getUpdateRepo());
    cJSON_AddStringToObject(update, "installedVersion", _sysInfo->getCurrentVersion());
    cJSON_AddStringToObject(update, "latestVersion", _latestVersion);
    cJSON_AddBoolToObject(update, "available", _hasRelease && compareVersions(_release.version, _sysInfo->getCurrentVersion()) > 0);
    cJSON_AddBoolToObject(update, "installable", _hasRelease && _release.assetUrl[0] != 0);
    cJSON_AddStringToObject(update, "releaseUrl", _hasRelease ? _release.releaseUrl : "");
    cJSON_AddNumberToObject(update, "progress", _progress);
    cJSON_AddStringToObject(update, "error", _error);
    xSemaphoreGive(_lock);
}

void UpdateCheck::setState(update_state_t state, const char *error)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    _state = state;
    copyString(_error, error, sizeof(_error));
    xSemaphoreGive(_lock);

    if (error)
        ESP_LOGE(TAG, "%s", error);
}

static esp_http_client_handle_t createClient(const char *url)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = github_ca_pem;
    config.timeout_ms = 20000;
    // GitHub redirects downloads to long signed URLs
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.user_agent = "HB-RF-ETH";
    config.disable_auto_redirect = true;
    return esp_http_client_init(&config);
}

// Opens the request and follows up to 5 redirects. Returns the HTTP status or -1.
static int openFollowingRedirects(esp_http_client_handle_t client, int64_t *contentLength)
{
    for (int i = 0; i < 6; i++)
    {
        if (esp_http_client_open(client, 0) != ESP_OK)
            return -1;

        *contentLength = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        if (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)
            return status;

        char discard[256];
        while (esp_http_client_read(client, discard, sizeof(discard)) > 0)
        {
        }
        if (esp_http_client_set_redirection(client) != ESP_OK)
            return -1;
    }
    return -1;
}

static int readHttp(void *ctx, char *buf, int len)
{
    return esp_http_client_read((esp_http_client_handle_t)ctx, buf, len);
}

// Same request as firmware 1.3.0: tells the original author's server which
// board, serial and version is running (his usage statistics). Only done while
// the update source is the original repository; the answer is not used, the
// firmware itself comes from the GitHub release.
void UpdateCheck::reportToAuthor()
{
    char url[160];
    snprintf(url, sizeof(url), "https://www.debmatic.de/hb-rf-eth/latestVersion.json?board=%d&serial=%s&version=%s",
             _sysInfo->getBoardType(), _sysInfo->getSerialNumber(), _sysInfo->getCurrentVersion());

    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = github_ca_pem; // Let's Encrypt, ISRG Root X1 is in the bundle
    config.timeout_ms = 10000;
    config.user_agent = "HB-RF-ETH";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
        return;

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Reported version to debmatic.de (HTTP %d)", esp_http_client_get_status_code(client));
    else
        ESP_LOGW(TAG, "Could not report version to debmatic.de: %s", esp_err_to_name(err));

    esp_http_client_cleanup(client);
}

void UpdateCheck::check()
{
    char url[160];
    char error[64];

    // one TLS session at a time, the heap is small
    if (strcasecmp(_settings->getUpdateRepo(), "alexreinert/HB-RF-ETH") == 0)
        reportToAuthor();
    snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", _settings->getUpdateRepo());

    ESP_LOGI(TAG, "Checking %s", url);
    setState(UPDATE_STATE_CHECKING);

    esp_http_client_handle_t client = createClient(url);
    if (client == NULL)
    {
        setState(UPDATE_STATE_ERROR, "Could not create HTTP client");
        return;
    }
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    int64_t contentLength = 0;
    int status = openFollowingRedirects(client, &contentLength);
    char *buffer = NULL;

    if (status != 200)
    {
        if (status == 404)
            snprintf(error, sizeof(error), "No release found in %s", _settings->getUpdateRepo());
        else if (status == 403 || status == 429)
            snprintf(error, sizeof(error), "GitHub rate limit reached (HTTP %d)", status);
        else if (status < 0)
            snprintf(error, sizeof(error), "Could not connect to GitHub");
        else
            snprintf(error, sizeof(error), "GitHub returned HTTP %d", status);
        setState(UPDATE_STATE_ERROR, error);
        goto cleanup;
    }

    {
        // GitHub sends ~4-5 KB; start small, grow if needed
        size_t capacity;
        size_t length;
        switch (readAllGrowing(readHttp, client, 2048, MAX_RELEASE_JSON, &buffer, &length, &capacity))
        {
        case READ_ALL_OK:
            break;
        case READ_ALL_NO_MEMORY:
            snprintf(error, sizeof(error), "Out of memory (largest free block %u bytes)", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            setState(UPDATE_STATE_ERROR, error);
            goto cleanup;
        case READ_ALL_TOO_LARGE:
            setState(UPDATE_STATE_ERROR, "Release information too large");
            goto cleanup;
        default:
            setState(UPDATE_STATE_ERROR, "Error reading the release information");
            goto cleanup;
        }
    }

    {
        release_info_t release;
        if (!parseRelease(buffer, &release))
        {
            setState(UPDATE_STATE_ERROR, "Latest release has no firmware .bin");
            goto cleanup;
        }

        xSemaphoreTake(_lock, portMAX_DELAY);
        _release = release;
        _hasRelease = true;
        copyString(_latestVersion, release.version, sizeof(_latestVersion));
        xSemaphoreGive(_lock);

        if (compareVersions(release.version, _sysInfo->getCurrentVersion()) > 0)
        {
            ESP_LOGW(TAG, "Firmware %s is available", release.version);
            setState(UPDATE_STATE_AVAILABLE);
        }
        else
        {
            ESP_LOGI(TAG, "Firmware is up to date (latest release %s)", release.version);
            setState(UPDATE_STATE_UP_TO_DATE);
        }
    }

cleanup:
    free(buffer);
    esp_http_client_cleanup(client);
}

bool UpdateCheck::download(const char *url, const char *sha256, int expectedSize)
{
    char error[64];
    bool ok = false;
    bool otaStarted = false;
    esp_ota_handle_t ota = 0;
    char *buffer = NULL;
    int total = 0;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    esp_http_client_handle_t client = createClient(url);

    int64_t contentLength = 0;
    int status = client ? openFollowingRedirects(client, &contentLength) : -1;
    if (status != 200)
    {
        snprintf(error, sizeof(error), "Download failed (HTTP %d)", status);
        setState(UPDATE_STATE_ERROR, error);
        goto cleanup;
    }

    if (contentLength > 0)
        expectedSize = (int)contentLength;
    if (expectedSize > 0 && (partition == NULL || (size_t)expectedSize > partition->size))
    {
        setState(UPDATE_STATE_ERROR, "Firmware does not fit into the update partition");
        goto cleanup;
    }

    if (esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota) != ESP_OK)
    {
        setState(UPDATE_STATE_ERROR, "Could not start the update");
        goto cleanup;
    }
    otaStarted = true;

    buffer = (char *)malloc(4096);
    if (buffer == NULL)
    {
        setState(UPDATE_STATE_ERROR, "Out of memory");
        goto cleanup;
    }

    for (;;)
    {
        int read = esp_http_client_read(client, buffer, 4096);
        if (read < 0)
        {
            setState(UPDATE_STATE_ERROR, "Error while downloading");
            goto cleanup;
        }
        if (read == 0)
            break;

        if (esp_ota_write(ota, buffer, read) != ESP_OK)
        {
            setState(UPDATE_STATE_ERROR, "Not a valid firmware image");
            goto cleanup;
        }
        mbedtls_sha256_update_ret(&sha, (unsigned char *)buffer, read);
        total += read;

        if (expectedSize > 0)
            _progress = (int)((int64_t)total * 100 / expectedSize);
    }

    if (expectedSize > 0 && total != expectedSize)
    {
        snprintf(error, sizeof(error), "Download incomplete (%d of %d bytes)", total, expectedSize);
        setState(UPDATE_STATE_ERROR, error);
        goto cleanup;
    }

    if (sha256 && *sha256)
    {
        unsigned char digest[32];
        char hex[65];
        mbedtls_sha256_finish_ret(&sha, digest);
        for (int i = 0; i < 32; i++)
            sprintf(hex + 2 * i, "%02x", digest[i]);
        if (strcmp(hex, sha256) != 0)
        {
            setState(UPDATE_STATE_ERROR, "SHA-256 checksum does not match");
            goto cleanup;
        }
    }

    otaStarted = false;
    if (esp_ota_end(ota) != ESP_OK)
    {
        setState(UPDATE_STATE_ERROR, "Firmware image is corrupt");
        goto cleanup;
    }

    if (esp_ota_set_boot_partition(partition) != ESP_OK)
    {
        setState(UPDATE_STATE_ERROR, "Could not activate the new firmware");
        goto cleanup;
    }

    ESP_LOGW(TAG, "Firmware written to %s (%d bytes)", partition->label, total);
    ok = true;

cleanup:
    if (otaStarted)
        esp_ota_abort(ota);
    free(buffer);
    mbedtls_sha256_free(&sha);
    if (client)
        esp_http_client_cleanup(client);
    return ok;
}

void UpdateCheck::install(bool force)
{
    release_info_t release;

    xSemaphoreTake(_lock, portMAX_DELAY);
    bool hasRelease = _hasRelease;
    release = _release;
    xSemaphoreGive(_lock);

    if (!hasRelease || release.assetUrl[0] == 0)
    {
        setState(UPDATE_STATE_ERROR, "No firmware to install, check for updates first");
        return;
    }
    if (!force && compareVersions(release.version, _sysInfo->getCurrentVersion()) <= 0)
    {
        setState(UPDATE_STATE_UP_TO_DATE);
        return;
    }

    ESP_LOGW(TAG, "Installing firmware %s from %s", release.version, release.assetUrl);
    _progress = 0;
    setState(UPDATE_STATE_INSTALLING);
    _statusLED->setState(LED_STATE_BLINK_FAST);

    if (download(release.assetUrl, release.sha256, release.assetSize))
    {
        _progress = 100;
        setState(UPDATE_STATE_INSTALLED);
        _statusLED->setState(LED_STATE_OFF);
        SystemControl::restart("update", 3000);
    }
    else
    {
        _progress = 0;
        _statusLED->setState(LED_STATE_OFF);
    }
}

void UpdateCheck::_taskFunc()
{
    uint32_t notification = 0;

    // some time for the initial network connection, unless something is requested earlier
    xTaskNotifyWait(0, ULONG_MAX, &notification, 30000 / portTICK_PERIOD_MS);

    for (;;)
    {
        if (notification & (NOTIFY_INSTALL | NOTIFY_INSTALL_FORCE))
        {
            install(notification & NOTIFY_INSTALL_FORCE);
        }
        else
        {
            check();

            if (_state == UPDATE_STATE_AVAILABLE)
            {
                _statusLED->setState(LED_STATE_BLINK_SLOW);
                if (_settings->getUpdateAuto())
                {
                    install(false);
                }
            }
            else if (_state == UPDATE_STATE_UP_TO_DATE)
            {
                _statusLED->setState(LED_STATE_OFF);
            }
        }

        // wait for the next check, re-reading the interval so a changed setting applies
        notification = 0;
        int64_t start = esp_timer_get_time();
        while (notification == 0 && (esp_timer_get_time() - start) < (int64_t)_settings->getUpdateInterval() * 3600LL * 1000000LL)
        {
            xTaskNotifyWait(0, ULONG_MAX, &notification, 60000 / portTICK_PERIOD_MS);
        }
    }

    vTaskDelete(NULL);
}
