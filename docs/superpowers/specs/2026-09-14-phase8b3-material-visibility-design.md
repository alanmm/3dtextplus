# Fase 8b-3 — Material: Ocultar + Reposicionar por Modo — Documento de Design

## 1. Objetivo

Terceira de 7 sub-fases do trabalho de "presets e valores padrão".
A aba Material passa a mostrar só os controles que têm efeito real no
modo selecionado, com os controles restantes se reposicionando pra
preencher o espaço (em vez de deixar um vão em branco).

## 2. Regras de visibilidade por modo

Verificado em `shaders/model.frag`, não assumido: `uMetalness` só é
lido no modo Metálico; `uRoughness` só é lido nos modos Metálico e
Vidro; `sample_env()`/`uEnvTex` só são chamados nos modos Metálico e
Vidro.

| Modo | Metalização | Rugosidade | Seção "Imagem de ambiente" |
|---|---|---|---|
| Metálico (padrão) | visível | visível | visível |
| Clássico | oculta | oculta | oculta |
| Vidro | oculta | visível | visível |
| Fosco | oculta | oculta | oculta |

## 3. Sem campos novos, sem "customizado"

`metalness` e `roughness` são campos **únicos e compartilhados** entre
os modos que os usam (não existe um `roughness` separado por modo) —
e o valor-alvo do usuário pra Rugosidade é o mesmo (0.5) tanto no
Metálico quanto no Vidro. Combinado com o fato de que, uma vez
ocultos, esses sliders só ficam interagíveis nos modos onde already
têm efeito real (a própria ocultação impede ajuste "invisível", mesmo
raciocínio que cancelou a Fase 8b-2) — não há necessidade de nenhum
mecanismo de "aplicar o padrão do modo ao selecioná-lo". A Fase 8b-1
já garante um valor de fábrica sensato pros dois campos.

**Isso não se generaliza pras próximas fases** (Fundo, Partículas) —
lá os campos compartilhados têm valores-alvo DIFERENTES por opção e
continuam interagíveis independente da opção escolhida, então
precisam de uma lógica de aplicar-padrão de verdade. Reavaliar caso a
caso quando chegar a hora.

## 4. Mecanismo de reposicionamento (novo nesta base de código)

Não existe hoje nenhum reposicionamento dinâmico de controles — todo
`.rc` é estático. Abordagem, pra não inventar constantes de espaçamento
novas nem lidar com conversão de unidades de diálogo manualmente:

1. Os 3 blocos (Metalização, Rugosidade, Ambiente) são definidos como
   listas de IDs de controle, na ordem em que já aparecem no `.rc`.
2. No `WM_INITDIALOG` da aba, **antes de qualquer ocultação**, a
   posição Y de cada controle (relativa ao próprio diálogo da aba, via
   `GetWindowRect` + `MapWindowPoints`) é capturada uma única vez e
   guardada — isso já vem em **pixels**, resolvido pelo próprio Win32 a
   partir do `.rc` original, sem precisar converter unidade de diálogo
   na mão.
3. Uma função `material_layout(HWND h)` roda sempre que o modo muda (e
   na inicialização): percorre os 3 blocos na ordem original,
   mantendo um cursor Y que começa na posição capturada do primeiro
   bloco. Bloco oculto: só `ShowWindow(SW_HIDE)` em todos os seus
   controles, cursor não avança. Bloco visível: reposiciona cada
   controle em `cursor + (deslocamento capturado dentro do bloco)`,
   depois avança o cursor pela altura do bloco **mais o espaçamento
   que já existia entre esse bloco e o próximo na disposição original**
   (também capturado, não um número novo inventado) — garante o mesmo
   respiro visual que o `.rc` já definia.

## 5. Testes

Sem cobertura automatizada (lógica 100% dentro de `config_dialog.c`,
mesmo padrão já estabelecido pro resto do diálogo). Verificação via
captura real da aba Material nos 4 modos, confirmando: os controles
certos aparecem/somem por modo, e os que ficam visíveis se
reposicionam sem sobreposição nem vão em branco.

## 6. Fora de escopo

Sheen/Anisotropy/IOR (decidido pular, ver memória do projeto); Fundo
(fase 4), Partículas (fase 5), mapa de ambiente embutido (fase 6),
presets (fase 7).
