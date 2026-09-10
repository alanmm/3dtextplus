# Modern 3D Text

Reescrita moderna do screensaver clássico "Texto 3D" do Windows. Nativo,
leve, OpenGL 3.3.

**Status:** Fase 2b — configuração pelo registro (`HKCU\Software\Modern3DText`) +
diálogo Win32 com abas **Conteúdo** (texto, fonte, negrito/itálico, cor) e
**Movimento** (profundidade, ângulo, inclinação, período), com **mini-preview 3D
ao vivo**. O texto 3D extrudado (fonte → contornos → tampa + paredes) e o pêndulo
limitado vêm da Fase 2a.

- Design completo: [`docs/superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md`](docs/superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md)
- Planos de implementação: [`docs/superpowers/plans/`](docs/superpowers/plans/)

## Build

Precisa do **w64devkit** (MinGW-w64 portátil, sem instalador):

1. Baixe de <https://github.com/skeeto/w64devkit/releases> e extraia em qualquer
   pasta (ex.: `C:\w64devkit` ou `C:\Users\<você>\w64devkit`).
2. Rode `w64devkit.exe` (abre um shell com `gcc`/`make`/`windres`/`gdb` no PATH),
   ou adicione `<pasta>\bin` ao seu PATH — o `gcc` precisa achar `as` e `ld`.
3. Na raiz do projeto:
   ```sh
   mingw32-make -f build/Makefile          # release -> dist/Modern3DText.scr
   mingw32-make -f build/Makefile debug
   mingw32-make -f build/Makefile test      # testes unitários
   mingw32-make -f build/Makefile run        # roda /s
   mingw32-make -f build/Makefile config     # roda /c
   ```

Versão do toolchain testada: ver [`toolchain.txt`](toolchain.txt).

## Instalar para testar

Copie `dist/Modern3DText.scr` para `C:\Windows\System32\` (precisa de admin), ou
clique com o botão direito no arquivo e escolha **Instalar** / **Testar**.

## Licença

MIT — ver [`LICENSE`](LICENSE).
