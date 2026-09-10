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

    /* --- RenderQuality: ladder --- */
    Config cfg;
    config_defaults(&cfg);      /* msaa 4, render_scale 1.0 */

    RenderQuality q0 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 0);
    EXPECT(q0.bloom == 1 && q0.msaa == 4 && q0.render_scale > 0.99f);
    RenderQuality q1 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 1);
    EXPECT(q1.bloom == 0 && q1.msaa == 4);
    RenderQuality q4 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 4);
    EXPECT(q4.msaa == 0 && q4.bloom == 0);
    RenderQuality q6 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 6);
    EXPECT(q6.render_scale <= 0.5f + 1e-4f);
    RenderQuality qhi = render_quality_for_step(&cfg, M3DT_TIER_FULL, 99);   /* clamp */
    EXPECT(qhi.render_scale <= 0.5f + 1e-4f && qhi.step == 6);
    EXPECT(render_quality_ladder_len(M3DT_TIER_FULL) == 7);

    RenderQuality qr = render_quality_for_step(&cfg, M3DT_TIER_REDUCED, 0);
    EXPECT(qr.bloom == 0 && qr.msaa <= 2 && qr.render_scale <= 0.75f + 1e-4f);
    EXPECT(render_quality_ladder_len(M3DT_TIER_REDUCED) == 1);
}
