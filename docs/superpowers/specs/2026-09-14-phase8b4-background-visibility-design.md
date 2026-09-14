# Fase 8b-4 — Fundo: Ocultar + Reposicionar + Cor 1 por Tipo — Documento de Design

## 1. Objetivo

Quarta de 7 sub-fases. A aba Fundo passa a mostrar só os controles
pertinentes ao tipo de fundo selecionado, com reposicionamento
(mesma técnica da Fase 8b-3), mais a correção pontual do único campo
realmente conflitante entre tipos: Cor 1.

## 2. Levantamento de conflitos (antes de desenhar)

Verificado contra o shader/uso real de cada campo, não assumido:

- **Cor 1** (`bg_color1_r/g/b`) — usada tanto por Sólido quanto por
  Gradiente, com valores-alvo DIFERENTES (Sólido: 14,12,12; Gradiente:
  16,10,10) → **único conflito real**.
- Cor 2 / Ângulo (`bg_color2_*`/`bg_grad_angle`) — só usados por
  Gradiente, Sólido nunca lê. Sem conflito.
- Ajuste / Velocidade de pan (`bg_image_fit`/`bg_pan_speed`) — só
  usados por Imagem. Sem conflito, e os valores-alvo do usuário já
  coincidem com o que `config_defaults()` já tem (Cobrir / 0.02) —
  nada a mudar além de ocultar/mostrar.
- Cores da Nebulosa (`bg_neb_color1/2_*`) — só usadas por Nebulosa.
  Sem conflito. Ficaram de fora da Fase 8b-1 de propósito (só se
  aplicam quando Nebulosa é selecionado, que não é o tipo padrão) —
  esta fase completa isso, corrigindo o valor de fábrica direto (sem
  precisar de mecanismo nenhum, já que não há conflito).

## 3. Regras de visibilidade por tipo

| Tipo | Cor 1 | Cor 2 + Ângulo | Imagem (bloco inteiro) | Nebulosa (2 cores) |
|---|---|---|---|---|
| Sólido | visível | oculto | oculto | oculto |
| Gradiente (padrão) | visível | visível | oculto | oculto |
| Imagem | oculto | oculto | visível | oculto |
| Nebulosa | oculto | oculto | oculto | visível |

4 blocos, na ordem em que já aparecem no `.rc`: **Cor1** (label +
botão), **Cor2+Ângulo** (label+botão, label+valor+slider), **Imagem**
(label+caminho+Escolher+Limpar+Ajuste-label+Ajuste-combo+
Velocidade-label+valor+slider — tudo um bloco só, já que todos só
importam no tipo Imagem), **Nebulosa** (label + 2 botões de cor).
Mesmo mecanismo de captura-e-reflow da Fase 8b-3 (`bg_layout_capture`/
`bg_layout_apply`), reaproveitado sem reinventar.

## 4. Campos de configuração

2 campos novos: `bg_solid_customized`, `bg_gradient_customized` (`int`,
`0/1`, padrão `0`).

**Regra**: ao selecionar Sólido com `bg_solid_customized == 0`, aplica
`bg_color1 = (0.05490, 0.04706, 0.04706)` (14,12,12). Ao selecionar
Gradiente com `bg_gradient_customized == 0`, aplica `bg_color1 =
(0.06275, 0.03922, 0.03922)` (16,10,10 — já é o valor de fábrica desde
a Fase 8b-1, então na prática só importa depois que Sólido tiver
sobrescrito o campo compartilhado). Escolher uma cor manualmente pelo
botão "Escolher cor..." de Cor 1 marca o campo do tipo ATUALMENTE
selecionado como customizado (`bg_solid_customized` ou
`bg_gradient_customized`, conforme `g_work.background_type` no
momento do clique).

**Limitação aceita, documentada**: como os dois tipos compartilham um
único campo de armazenamento (não existem campos separados
"cor de Sólido" e "cor de Gradiente"), customizar os dois tipos de
forma independente e alternar entre eles mostra a última cor
escolhida, não duas memórias separadas — mesmo comportamento comum em
qualquer campo compartilhado entre modos. O que a flag realmente
garante é só a primeira visita a cada tipo mostrar a cor de fábrica
certa daquele tipo, não a sobra do outro.

## 5. Testes

Round-trip + clamp dos 2 campos novos em `test_config.c`. Verificação
visual real dos 4 tipos (layout) + do cenário de conflito (visitar
Sólido pela 1ª vez deve mostrar 14,12,12; voltar pra Gradiente sem
nunca ter customizado nenhum dos dois deve mostrar 16,10,10 de novo).

## 6. Fora de escopo

Partículas (fase 5), mapa de ambiente embutido (fase 6), presets
(fase 7).
