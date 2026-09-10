#include "util/log.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static FILE *g_log;

void log_format_line(char *buf, int n, const char *lvl, const char *msg,
                     int Y, int Mo, int D, int h, int mi, int s)
{
    snprintf(buf, (size_t)n, "%04d-%02d-%02d %02d:%02d:%02d [%s] %s\n",
             Y, Mo, D, h, mi, s, lvl, msg);
}

static void log_dir_path(char *out, size_t n)
{
    char base[MAX_PATH];
    DWORD k = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
    if (k == 0 || k >= sizeof base) {
        snprintf(out, n, ".");
        return;
    }
    snprintf(out, n, "%s\\Modern3DText", base);
}

void log_init(void)
{
    char dir[MAX_PATH + 32];
    log_dir_path(dir, sizeof dir);
    CreateDirectoryA(dir, NULL);

    char path[MAX_PATH + 64];
    snprintf(path, sizeof path, "%s\\log.txt", dir);
    g_log = fopen(path, "a");
    log_infof("---- log_init ----");
}

void log_shutdown(void)
{
    if (g_log) { fclose(g_log); g_log = NULL; }
}

static void log_v(const char *lvl, const char *fmt, va_list ap)
{
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);

    SYSTEMTIME t;
    GetLocalTime(&t);

    char line[1200];
    log_format_line(line, (int)sizeof line, lvl, msg,
                    t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    if (g_log) { fputs(line, g_log); fflush(g_log); }
#ifdef DEBUG
    OutputDebugStringA(line);
#endif
}

void log_infof(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); log_v("INFO", fmt, ap); va_end(ap);
}

void log_errorf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); log_v("ERROR", fmt, ap); va_end(ap);
}
