# Fase 8b-1 — Novos Valores Padrão — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir os valores de `config_defaults()` pelos novos
valores-alvo definidos pelo usuário — puramente numérico, sem mudança
de struct nem de UI.

**Architecture:** Um único arquivo muda (`src/config.c`), mais os
testes que verificam o comportamento (`build/tests/test_config.c`).

**Tech Stack:** C11.

## Global Constraints

- Nenhum campo novo na `Config`, nenhuma mudança de UI.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da raiz,
  com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile test`.

---

### Task 1: Atualizar `config_defaults()` + testes

**Files:**
- Modify: `src/config.c`
- Modify: `build/tests/test_config.c`

**Interfaces:** nenhuma — só valores literais dentro de
`config_defaults()`, já consumida em toda parte sem mudança de
assinatura.

- [ ] **Step 1: Novo `config_defaults()`**

Trocar o corpo inteiro da função (de `memset` até o `}` final) por:

```c
void config_defaults(Config *c)
{
    memset(c, 0, sizeof *c);
    c->version = CFG_VERSION;
    c->content_mode = CONTENT_TEXT;
    strcpy(c->text, "3D Text+");
    wcscpy(c->font_family, L"Segoe UI");
    c->font_bold = 1;
    c->font_italic = 0;
    c->depth = 0.10f;
    c->max_angle_y = 42.0f;
    c->tilt_x = 8.0f;
    c->period = 9.0f;
    c->base_r = 0.72157f; c->base_g = 0.74118f; c->base_b = 0.78039f;
    c->material_mode = 1;
    c->metalness = 0.9f;
    c->roughness = 0.5f;
    c->env_path[0] = 0;
    c->bevel_mode = 1;
    c->bevel_size = 0.005f;
    c->bevel_depth = 0.007f;
    c->bevel_segments = 6;
    c->shell = 0;
    c->wall_thickness = 0.015f;
    c->quality = 2;
    c->bloom_on = 1;
    c->bloom_threshold = 0.64f;
    c->bloom_intensity = 0.65f;
    c->bloom_radius = 0.27f;
    c->fps_cap = 60;
    c->vsync = 1;
    c->msaa = 4;
    c->render_scale = 1.0f;
    c->auto_quality = 1;
    c->streaks_mode = 2;
    c->streaks_intensity = 0.76f;
    c->streaks_length = 0.14f;
    c->chroma_on = 1;
    c->chroma_strength = 0.52f;
    c->vignette_on = 1;
    c->vignette_amount = 0.35f;
    c->fxaa_on = 1;
    c->background_type = 1;
    c->bg_color1_r = 0.06275f; c->bg_color1_g = 0.03922f; c->bg_color1_b = 0.03922f;
    c->bg_color2_r = 0.04706f; c->bg_color2_g = 0.07451f; c->bg_color2_b = 0.13725f;
    c->bg_grad_angle = 101.0f;
    c->bg_image_path[0] = 0;
    c->bg_image_fit = 0;
    c->bg_pan_speed = 0.02f;
    c->bg_neb_color1_r = 0.03f; c->bg_neb_color1_g = 0.02f; c->bg_neb_color1_b = 0.08f;
    c->bg_neb_color2_r = 0.25f; c->bg_neb_color2_g = 0.10f; c->bg_neb_color2_b = 0.35f;
    c->particles_on = 1;
    c->particles_kind = 0;
    c->particles_density = 0.43f;
    c->particles_speed = 0.76f;
    c->particles_size_scale = 0.8f;
    c->particles_opacity = 0.5f;
    c->clock_show_date = 0;
    c->clock_show_seconds = 0;
    c->svg_path[0] = 0;
    c->svg_color_mode = 0;
    c->mesh_path[0] = 0;
    c->mesh_size_scale = 1.0f;
    c->mesh_use_file_materials = 0;
    c->ui_language = 0;
}
```

(`bg_neb_color1/2` e `bg_image_fit`/`bg_pan_speed` ficam com os valores
antigos — são só usados quando `background_type` = Nebulosa/Imagem,
fora de escopo desta sub-fase per o spec §2.)

- [ ] **Step 2: Atualizar o bloco "defaults" em `test_config.c`**

Trocar:

```c
    /* defaults */
    Config d;
    config_defaults(&d);
    EXPECT(strcmp(d.text, "Modern 3D Text") == 0);
    EXPECT(wcscmp(d.font_family, L"Segoe UI") == 0);
    EXPECT(nearf(d.depth, 0.30f));
    EXPECT(d.version == 2);
    EXPECT(d.background_type == 0);
    EXPECT(nearf(d.bg_color1_r, 0.02f) && nearf(d.bg_color1_g, 0.03f) && nearf(d.bg_color1_b, 0.05f));
    EXPECT(d.particles_on == 0);
    EXPECT(nearf(d.particles_density, 0.5f));
    EXPECT(nearf(d.particles_opacity, 1.0f));
    EXPECT(d.content_mode == CONTENT_TEXT);
    EXPECT(d.clock_show_date == 0);
    EXPECT(d.clock_show_seconds == 0);
    EXPECT(d.svg_path[0] == 0);
    EXPECT(d.svg_color_mode == 0);
    EXPECT(d.mesh_path[0] == 0);
    EXPECT(nearf(d.mesh_size_scale, 1.0f));
    EXPECT(d.mesh_use_file_materials == 0);
    EXPECT(d.ui_language == 0);
```

por:

```c
    /* defaults */
    Config d;
    config_defaults(&d);
    EXPECT(strcmp(d.text, "3D Text+") == 0);
    EXPECT(wcscmp(d.font_family, L"Segoe UI") == 0);
    EXPECT(d.font_bold == 1);
    EXPECT(d.font_italic == 0);
    EXPECT(nearf(d.depth, 0.10f));
    EXPECT(nearf(d.max_angle_y, 42.0f));
    EXPECT(nearf(d.tilt_x, 8.0f));
    EXPECT(nearf(d.period, 9.0f));
    EXPECT(nearf(d.base_r, 0.72157f) && nearf(d.base_g, 0.74118f) && nearf(d.base_b, 0.78039f));
    EXPECT(d.version == 2);
    EXPECT(d.material_mode == 1);
    EXPECT(nearf(d.metalness, 0.9f));
    EXPECT(nearf(d.roughness, 0.5f));
    EXPECT(d.bevel_mode == 1);
    EXPECT(nearf(d.bevel_size, 0.005f));
    EXPECT(nearf(d.bevel_depth, 0.007f));
    EXPECT(d.bevel_segments == 6);
    EXPECT(d.shell == 0);
    EXPECT(nearf(d.wall_thickness, 0.015f));
    EXPECT(d.quality == 2);
    EXPECT(d.bloom_on == 1);
    EXPECT(nearf(d.bloom_threshold, 0.64f));
    EXPECT(nearf(d.bloom_intensity, 0.65f));
    EXPECT(nearf(d.bloom_radius, 0.27f));
    EXPECT(d.streaks_mode == 2);
    EXPECT(nearf(d.streaks_intensity, 0.76f));
    EXPECT(nearf(d.streaks_length, 0.14f));
    EXPECT(d.chroma_on == 1);
    EXPECT(nearf(d.chroma_strength, 0.52f));
    EXPECT(d.vignette_on == 1);
    EXPECT(nearf(d.vignette_amount, 0.35f));
    EXPECT(d.fxaa_on == 1);
    EXPECT(d.background_type == 1);
    EXPECT(nearf(d.bg_color1_r, 0.06275f) && nearf(d.bg_color1_g, 0.03922f) && nearf(d.bg_color1_b, 0.03922f));
    EXPECT(nearf(d.bg_color2_r, 0.04706f) && nearf(d.bg_color2_g, 0.07451f) && nearf(d.bg_color2_b, 0.13725f));
    EXPECT(nearf(d.bg_grad_angle, 101.0f));
    EXPECT(d.particles_on == 1);
    EXPECT(d.particles_kind == 0);
    EXPECT(nearf(d.particles_density, 0.43f));
    EXPECT(nearf(d.particles_speed, 0.76f));
    EXPECT(nearf(d.particles_size_scale, 0.8f));
    EXPECT(nearf(d.particles_opacity, 0.5f));
    EXPECT(d.content_mode == CONTENT_TEXT);
    EXPECT(d.clock_show_date == 0);
    EXPECT(d.clock_show_seconds == 0);
    EXPECT(d.svg_path[0] == 0);
    EXPECT(d.svg_color_mode == 0);
    EXPECT(d.mesh_path[0] == 0);
    EXPECT(nearf(d.mesh_size_scale, 1.0f));
    EXPECT(d.mesh_use_file_materials == 0);
    EXPECT(d.ui_language == 0);
```

- [ ] **Step 3: Build + testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`.

- [ ] **Step 4: Verificação visual real**

**Aviso ao usuário**: tentar `PushNotification` antes; se vier "not
sent", mandar mensagem de chat e **esperar confirmação explícita**
antes de rodar qualquer captura real.

```bash
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "0"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_new_defaults.png"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 10
```

Sem chave de registro (instalação "limpa"), confirma visualmente: texto
"3D Text+", material metálico com reflexo perceptível, bevel geométrico
visível (facetado, não suave), fundo em gradiente escuro-avermelhado
virando azulado, bloom presente, streaks anamórficos, partículas de
poeira ativas.

- [ ] **Step 5: Build release + refresh do `.exe`**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 6: Commit**

```bash
git add src/config.c build/tests/test_config.c
git commit -m "feat: update factory-default config to the new baseline"
```

---

## Self-Review

1. **Cobertura do spec**: toda linha da tabela do spec (§2) tem uma
   atribuição correspondente no novo `config_defaults()`; os campos
   marcados "já batem, sem mudança" foram deixados como estavam.
2. **Placeholders**: nenhum — o corpo completo da função e do bloco de
   teste está escrito por extenso, sem elipses nem "..." (exceto o
   comentário explicando `bg_neb_*`/`bg_image_fit`/`bg_pan_speed`
   ficarem intocados, que é intencional e documentado no próprio
   spec §2 "fora de escopo").
3. **Consistência**: os valores usados no Step 1 e no Step 2 são
   idênticos (mesmas 5 casas decimais nas cores convertidas) — testados
   com a mesma tolerância `nearf` (1e-3) já usada em todo o arquivo.
