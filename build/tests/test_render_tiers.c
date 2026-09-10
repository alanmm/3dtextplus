#include "test.h"
#include "render_tiers.h"
#include <string.h>

void run_render_tiers_tests(void)
{
    /* --- classificacao estatica por GL_RENDERER --- */
    EXPECT(m3dt_tier_detect(NULL) == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("NVIDIA GeForce RTX 5060 Ti/PCIe/SSE2") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("Intel(R) UHD Graphics 620") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("AMD Radeon RX 6600") == M3DT_TIER_FULL);

    EXPECT(m3dt_tier_detect("GDI Generic") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("llvmpipe (LLVM 15.0.7, 256 bits)") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("softpipe") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("Microsoft Basic Render Driver") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("D3D12 (WARP)") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("Google SwiftShader") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("GDI GENERIC") == M3DT_TIER_REDUCED);   /* case-insensitive */

    EXPECT(strcmp(m3dt_tier_name(M3DT_TIER_FULL), "full") == 0);
    EXPECT(strcmp(m3dt_tier_name(M3DT_TIER_REDUCED), "reduced") == 0);
}
