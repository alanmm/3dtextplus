#include "update_check.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define KEY_MAIN L"Software\\3DTextPlus"
#define VAL_LAST L"last_update_check"

int update_parse_version(const char *s, int out[3])
{
    out[0] = out[1] = out[2] = 0;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == 'v' || *s == 'V') ++s;
    if (*s < '0' || *s > '9') return 0;

    for (int i = 0; i < 3; ++i) {
        long v = 0;
        while (*s >= '0' && *s <= '9') {
            if (v < 100000) v = v * 10 + (*s - '0');
            ++s;
        }
        out[i] = (int)v;
        if (*s != '.' || s[1] < '0' || s[1] > '9') break;
        ++s;
    }
    return 1;
}

int update_is_newer(const int latest[3], const int current[3])
{
    for (int i = 0; i < 3; ++i) {
        if (latest[i] != current[i]) return latest[i] > current[i];
    }
    return 0;
}

int update_extract_tag(const char *json, char *tag, int cap)
{
    if (!json || !tag || cap < 2) return 0;
    const char *p = strstr(json, "\"tag_name\"");
    if (!p) return 0;
    p += 10;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return 0;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '"') return 0;
    ++p;

    int n = 0;
    while (*p && *p != '"') {
        char c = *p++;
        int ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                 || c == '.' || c == '_' || c == '-';
        if (!ok || n >= cap - 1) return 0;
        tag[n++] = c;
    }
    if (*p != '"' || n == 0) return 0;
    tag[n] = 0;
    return 1;
}

int update_due(long long last, long long now, int interval_days)
{
    if (last <= 0 || now < last) return 1;
    return (now - last) >= (long long)interval_days * 86400LL;
}

long long update_last_check_load_from(const wchar_t *key)
{
    wchar_t buf[32];
    DWORD sz = sizeof buf;
    if (RegGetValueW(HKEY_CURRENT_USER, key, VAL_LAST, RRF_RT_REG_SZ, NULL, buf, &sz) != ERROR_SUCCESS)
        return 0;
    buf[31] = 0;
    return _wcstoi64(buf, NULL, 10);
}

void update_last_check_save_to(const wchar_t *key, long long t)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return;
    wchar_t buf[32];
    swprintf(buf, 32, L"%lld", t);
    RegSetValueExW(k, VAL_LAST, 0, REG_SZ, (const BYTE *)buf, (DWORD)((wcslen(buf) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

long long update_last_check_load(void) { return update_last_check_load_from(KEY_MAIN); }
void      update_last_check_save(long long t) { update_last_check_save_to(KEY_MAIN, t); }
