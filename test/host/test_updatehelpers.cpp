// Host unit test for include/updatehelpers.h
// Build and run: test/host/run.sh
#include <stdio.h>
#include <assert.h>
#include "updatehelpers.h"

static char *readFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    assert(fread(buf, 1, len, f) == (size_t)len);
    buf[len] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    // versions
    assert(compareVersions("1.3.0", "1.3.0") == 0);
    assert(compareVersions("v1.3.0", "1.3.0") == 0);
    assert(compareVersions("1.3.1", "1.3.0") > 0);
    assert(compareVersions("1.10.0", "1.9.9") > 0); // the old strcmp check got this wrong
    assert(compareVersions("1.3.0", "1.4.0") < 0);
    assert(compareVersions("2.0", "1.9.9") > 0);
    assert(compareVersions("1.4.0-beta", "1.4.0") == 0);

    // real upstream release JSON (saved from the GitHub API)
    release_info_t info;
    char *json = readFile(argv[1]);
    assert(parseRelease(json, &info));
    assert(strcmp(info.version, "1.3.0") == 0);
    assert(strstr(info.assetUrl, "firmware_1_3_0.bin") != NULL);
    assert(strstr(info.releaseUrl, "/releases/tag/") != NULL);
    assert(info.assetSize > 100000);
    free(json);

    // digest, bootloader skipped, no leading v
    assert(parseRelease("{\"tag_name\":\"1.4.0\",\"html_url\":\"h\",\"assets\":["
                        "{\"name\":\"bootloader.bin\",\"browser_download_url\":\"b\",\"size\":1},"
                        "{\"name\":\"firmware_1_4_0.bin\",\"browser_download_url\":\"https://x/f.bin\",\"size\":900000,"
                        "\"digest\":\"sha256:ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789\"}]}",
                        &info));
    assert(strcmp(info.assetUrl, "https://x/f.bin") == 0);
    assert(strcmp(info.sha256, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789") == 0);
    assert(info.assetSize == 900000);

    // no firmware asset / no tag / garbage
    assert(!parseRelease("{\"tag_name\":\"1.4.0\",\"assets\":[{\"name\":\"src.zip\",\"browser_download_url\":\"z\"}]}", &info));
    assert(!parseRelease("{\"message\":\"Not Found\"}", &info));
    assert(!parseRelease("not json", &info));

    printf("all updatehelpers tests passed\n");
    return 0;
}
