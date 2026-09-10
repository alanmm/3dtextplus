# QA — Fase 3a (materiais)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make`, `make debug`, `make test` verdes, sem warnings; `.scr` ~360 KB (< 3 MB).
- [x] `test_config`: round-trip de `material_mode` / `metalness` / `roughness` / `env_path`;
      `material_mode` "7" → 0 (clamp), `metalness` "5" → [0,1]. `reg_get_i` aceita REG_DWORD.
- [x] 4 PNGs (`text=ProArt`, um por modo) — `docs/img/phase3a-mat{0..3}.png`:
  - **0** especular clássico (como a Fase 2a);
  - **1** metálico prateado refletindo o ambiente procedural;
  - **2** vidro translúcido (dá pra ver as faces de trás através da frente, brilho de borda);
  - **3** fosco chapado, paredes escurecidas por AO.
  Sem `glError` no log.
- [x] `env_path` apontando para uma imagem equiretangular de teste: o modo 1 reflete a
      imagem (tons quentes do horizonte nas faces baixas); `env_path` vazio → volta ao
      procedural. Joelho suave no shader evita estouro do reflexo.
- [x] `/c` selftest: aba **Material** abre (combo + 2 sliders + picker), mini-preview
      renderiza, `exit=0`, sem `glError`.

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Material**: trocar o modo no combo → mini-preview muda na hora.
- [ ] Sliders **Metalização** / **Rugosidade** → o reflexo do preview responde ao vivo;
      rótulos numéricos acompanham.
- [ ] **Escolher...** abre `GetOpenFileNameW`; a imagem escolhida aparece refletida no
      preview; **Limpar** volta ao procedural; o caminho aparece com `...` no meio.
- [ ] `OK` grava `material_mode` / `metalness` / `roughness` / `env_path` (conferir no
      `regedit`); reabrir `/c` mostra os valores; `/s` (Visualizar) usa o material salvo.
- [ ] Modo vidro em tela cheia: a transparência não pisca feio ao girar (aceita erro
      mínimo de ordenação — WBOIT é Fase 7).
- [ ] 100 / 150 / 200 % DPI: aba Material sem corte.

## Notas conhecidas (fases posteriores)

- Ambiente padrão é **procedural** (céu/chão + 2 luzes de estúdio no shader) — sem
  matcap embutido. Imagem equiretangular é a opção `env_path`.
- Vidro = 1 passe transparente (blend + depth-mask off). WBOIT próprio = Fase 7.
- Sem tonemap "de verdade" — só um joelho no shader. Tonemap ACES + bloom = Fase 4.
- Roughness com imagem usa LOD de mipmap (aproximação, não pré-filtro de irradiância).
- Fresnel/energia não fisicamente conservados — é um material "de screensaver", tunado a olho.
