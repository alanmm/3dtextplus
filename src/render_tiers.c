#include "render_tiers.h"
#include "util/log.h"

#include <windows.h>
#include <ctype.h>
#include <string.h>

static int contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    size_t nl = strlen(needle);
    if (nl == 0) return 0;
    for (const char *p = hay; *p; ++p) {
        size_t i = 0;
        while (i < nl && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == nl) return 1;
    }
    return 0;
}

M3dtTier m3dt_tier_detect(const char *r)
{
    static const char *bad[] = {
        "GDI Generic", "llvmpipe", "softpipe", "swrast",
        "Microsoft Basic Render", "WARP", "SwiftShader", "Gallium llvmpipe",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        if (contains_ci(r, bad[i]))
            return M3DT_TIER_REDUCED;
    return M3DT_TIER_FULL;
}

int m3dt_tier_forced(M3dtTier *out)
{
    char b[16];
    DWORD n = GetEnvironmentVariableA("M3DT_FORCE_TIER", b, sizeof b);
    if (n == 0 || n >= sizeof b) return 0;
    if (contains_ci(b, "reduced")) { *out = M3DT_TIER_REDUCED; return 1; }
    if (contains_ci(b, "full"))    { *out = M3DT_TIER_FULL;    return 1; }
    return 0;
}

int m3dt_is_remote_session(void)
{
    return GetSystemMetrics(SM_REMOTESESSION) != 0;
}

const char *m3dt_tier_name(M3dtTier t)
{
    return t == M3DT_TIER_REDUCED ? "reduced" : "full";
}

M3dtTier m3dt_tier_resolve(const char *gl_renderer)
{
    M3dtTier t;
    if (m3dt_tier_forced(&t)) {
        log_infof("tier: %s (M3DT_FORCE_TIER)", m3dt_tier_name(t));
        return t;
    }
    if (m3dt_is_remote_session()) {
        log_infof("tier: reduced (sessao remota)");
        return M3DT_TIER_REDUCED;
    }
    t = m3dt_tier_detect(gl_renderer);
    log_infof("tier: %s (renderer='%s')", m3dt_tier_name(t),
              gl_renderer ? gl_renderer : "?");
    return t;
}
