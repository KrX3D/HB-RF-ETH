/* 
 *  updatehelpers.h is part of the HB-RF-ETH firmware - https://github.com/alexreinert/HB-RF-ETH
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

// Helpers for the online update check, kept free of ESP-IDF dependencies
// (except cJSON) so they can be unit tested on the host, see test/host.

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

typedef struct
{
    char version[33];       // tag name without a leading "v"
    char releaseUrl[160];   // html_url of the release
    char assetUrl[256];     // browser_download_url of the firmware .bin
    char sha256[65];        // lower case hex digest of the asset, empty if GitHub does not provide one
    int assetSize;          // size of the asset in bytes, 0 if unknown
} release_info_t;

// Compares two "major.minor.patch" versions, a leading "v" is ignored.
// Returns <0 if a < b, 0 if equal and >0 if a > b.
static inline int compareVersions(const char *a, const char *b)
{
    if (*a == 'v' || *a == 'V')
        a++;
    if (*b == 'v' || *b == 'V')
        b++;

    for (int i = 0; i < 3; i++)
    {
        long va = strtol(a, (char **)&a, 10);
        long vb = strtol(b, (char **)&b, 10);
        if (va != vb)
            return va < vb ? -1 : 1;
        if (*a == '.')
            a++;
        if (*b == '.')
            b++;
    }
    return 0;
}

static inline bool endsWith(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcasecmp(s + ls - lx, suffix) == 0;
}

static inline void copyString(char *dest, const char *src, size_t size)
{
    strncpy(dest, src ? src : "", size - 1);
    dest[size - 1] = 0;
}

// Parses the JSON of https://api.github.com/repos/<owner>/<repo>/releases/latest.
// Picks the first .bin asset that is not a bootloader or partition table image.
// Returns false if the JSON has no tag name or no firmware asset.
static inline bool parseRelease(const char *json, release_info_t *info)
{
    memset(info, 0, sizeof(*info));

    cJSON *root = cJSON_Parse(json);
    if (root == NULL)
        return false;

    bool ok = false;
    const char *tag = cJSON_GetStringValue(cJSON_GetObjectItem(root, "tag_name"));
    if (tag != NULL && *tag)
    {
        if (*tag == 'v' || *tag == 'V')
            tag++;
        copyString(info->version, tag, sizeof(info->version));
        copyString(info->releaseUrl, cJSON_GetStringValue(cJSON_GetObjectItem(root, "html_url")), sizeof(info->releaseUrl));

        cJSON *asset;
        cJSON_ArrayForEach(asset, cJSON_GetObjectItem(root, "assets"))
        {
            const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "name"));
            const char *url = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "browser_download_url"));
            if (name == NULL || url == NULL || !endsWith(name, ".bin") || strstr(name, "bootloader") || strstr(name, "partition"))
                continue;
            if (strlen(url) >= sizeof(info->assetUrl))
                continue;

            copyString(info->assetUrl, url, sizeof(info->assetUrl));

            cJSON *size = cJSON_GetObjectItem(asset, "size");
            info->assetSize = cJSON_IsNumber(size) ? size->valueint : 0;

            const char *digest = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "digest"));
            if (digest != NULL && strncmp(digest, "sha256:", 7) == 0 && strlen(digest + 7) == 64)
            {
                for (int i = 0; i < 64; i++)
                    info->sha256[i] = (char)tolower((unsigned char)digest[7 + i]);
                info->sha256[64] = 0;
            }

            ok = true;
            break;
        }
    }

    cJSON_Delete(root);
    return ok;
}
