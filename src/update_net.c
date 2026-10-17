#include "update_check.h"

#include <windows.h>
#include <wininet.h>

#define RELEASES_URL L"https://api.github.com/repos/alanmm/3dtextplus/releases/latest"

int update_fetch_latest_tag(char *tag, int cap)
{
    int ok = 0;
    HINTERNET net = InternetOpenW(L"3DTextPlus-UpdateCheck", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!net) return 0;

    DWORD timeout_ms = 8000;
    InternetSetOptionW(net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof timeout_ms);
    InternetSetOptionW(net, INTERNET_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof timeout_ms);
    InternetSetOptionW(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof timeout_ms);

    static const wchar_t headers[] = L"Accept: application/vnd.github+json\r\n";
    HINTERNET url = InternetOpenUrlW(net, RELEASES_URL, headers, (DWORD)-1,
                                     INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI |
                                     INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    if (url) {
        DWORD status = 0, sz = sizeof status;
        if (HttpQueryInfoW(url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &sz, NULL)
            && status == 200) {
            /* tag_name vem logo no inicio do JSON; o resto (assets, corpo) nao interessa */
            static char buf[32768];
            DWORD total = 0, got = 0;
            while (total < sizeof buf - 1 &&
                   InternetReadFile(url, buf + total, (DWORD)(sizeof buf - 1 - total), &got) && got)
                total += got;
            buf[total] = 0;
            ok = update_extract_tag(buf, tag, cap);
        }
        InternetCloseHandle(url);
    }
    InternetCloseHandle(net);
    return ok;
}
