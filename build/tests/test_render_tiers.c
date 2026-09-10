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
    EXPECT(q0.streaks == 1 && q0.bloom == 1 && q0.msaa == 4 && q0.render_scale > 0.99f);
    RenderQuality q1 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 1);
    EXPECT(q1.streaks == 0 && q1.bloom == 1);
    RenderQuality q2 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 2);
    EXPECT(q2.streaks == 0 && q2.bloom == 0 && q2.msaa == 4);
    RenderQuality q5 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 5);
    EXPECT(q5.msaa == 0);
    RenderQuality q7 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 7);
    EXPECT(q7.render_scale <= 0.5f + 1e-4f);
    RenderQuality qhi = render_quality_for_step(&cfg, M3DT_TIER_FULL, 99);   /* clamp */
    EXPECT(qhi.step == 7);
    EXPECT(render_quality_ladder_len(M3DT_TIER_FULL) == 8);

    RenderQuality qr = render_quality_for_step(&cfg, M3DT_TIER_REDUCED, 0);
    EXPECT(qr.streaks == 0 && qr.bloom == 0 && qr.msaa <= 2 && qr.render_scale <= 0.75f + 1e-4f);
    EXPECT(render_quality_ladder_len(M3DT_TIER_REDUCED) == 1);

    /* --- aq_decide: ladder + histerese + zona morta --- */
    AqInput a = { 0, 8, 30.0, 6.0 };
    EXPECT(aq_decide(a) == 1);                          /* lento -> degrada */
    a.since_change_s = 2.0;
    EXPECT(aq_decide(a) == 0);                          /* histerese trava */
    AqInput b = { 3, 8, 8.0, 9.0 };
    EXPECT(aq_decide(b) == 2);                          /* folgado -> recupera */
    AqInput c = { 7, 8, 99.0, 9.0 };
    EXPECT(aq_decide(c) == 7);                          /* ja no fundo do ladder */
    AqInput d = { 0, 8, 8.0, 9.0 };
    EXPECT(aq_decide(d) == 0);                          /* ja no topo */
    AqInput e = { 2, 8, 16.0, 9.0 };
    EXPECT(aq_decide(e) == 2);                          /* zona morta 12..22 */
}
