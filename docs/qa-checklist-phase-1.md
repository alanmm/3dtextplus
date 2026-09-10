# QA manual — Fase 1 (esqueleto do .scr)

Buildar (shell do w64devkit, `<w64devkit>\bin` no PATH, na raiz do projeto):

```sh
mingw32-make -f build/Makefile          # release -> dist/Modern3DText.scr
mingw32-make -f build/Makefile test
```

## Já verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make` compila os 5 `.c` sem warnings (`-O2 -Wall -Wextra`), `windres` OK, linka `dist/Modern3DText.scr` (~56 KB, < 3 MB).
- [x] `make debug` e `make test` (todas as asserções passam: 16 de cmdline, 2 de log).
- [x] Contexto **OpenGL 3.3 core** criado; `glClear` + `SwapBuffers` sem `glGetError`.
- [x] `/s` (via `M3DT_SELFTEST=1`, janela + auto-saída após 90 frames): loop de render roda, saída limpa, sem crash.
- [x] `/p <hwnd>` com janela-pai descartável: modo PREVIEW parseado, janela-filha renderiza, **encerra ~1 s após o pai ser destruído**, `rc=0`.
- [x] `/c` (via `M3DT_SELFTEST=1`, diálogo auto-fecha em 800 ms): recurso `IDD_CONFIG` carrega do `.res` embutido, loop modal roda e retorna.

> **Nota sobre testes:** lançar o `.scr` por `Start-Process` / `cmd /c` passa pelos
> *verbos de shell* de screensaver, que forçam `/s` e descartam outros argumentos.
> Para testar `/p` e `/c` com argumentos, invoque o binário **diretamente**
> (renomeie para `.exe`, ou chame pelo caminho no PowerShell com `&`), ou use o
> diálogo real do Windows (que invoca corretamente).

## Falta verificar (precisa de sessão interativa / olho humano)

### Instalação de teste
- [ ] Copiar `dist/Modern3DText.scr` para `C:\Windows\System32\` (admin).
- [ ] "Modern3DText" aparece na lista de proteções de tela do Windows.

### /c (config) — pelo diálogo do Windows
- [ ] Botão **Configurações...** abre o diálogo (parented à janela de configurações).
- [ ] `Modern3DText.scr` (duplo clique) e o botão direito → **Configurar** abrem o diálogo.
- [ ] OK / Cancelar / Esc / `X` fecham sem travar.
- [ ] 100 % / 150 % / 200 % DPI: sem corte de texto (manifest per-monitor v2).

### /p (preview) — pelo diálogo do Windows
- [ ] A mini-tela do diálogo de Proteção de Tela mostra a cor animada (ciano→azul→roxo, escura).
- [ ] Fechar o diálogo não deixa `Modern3DText.scr` órfão (Gerenciador de Tarefas).
- [ ] Selecionar outra proteção de tela e voltar: preview reinicia sem erro.

### /s (saver) — real, em tela cheia
- [ ] Preenche **todos** os monitores, topmost, sem barra de título.
- [ ] Multi-monitor com resoluções/orientações diferentes: cada tela cobre 100 %.
- [ ] Sai ao: mover o mouse > ~4 px · qualquer tecla · clique (esq/dir/meio) · roda do mouse.
- [ ] Tremor de 1–2 px **não** encerra.
- [ ] Cursor some ao rodar, volta ao sair.
- [ ] `Win+L` durante a execução: sem crash; ao voltar, comportamento sensato.

### Robustez
- [ ] `%LOCALAPPDATA%\Modern3DText\log.txt` registra `start`/`mode`/`exit` e a versão do contexto GL.
- [ ] Rodar `/s` (via selftest ou real) 20× seguidas: sem vazamento de processo/handle (Process Explorer — handles GDI/USER estáveis).
- [ ] VM ou RDP sem GPU dedicada: cai para contexto 3.1 / 2.1 / legado e ainda limpa a tela e sai no input.
