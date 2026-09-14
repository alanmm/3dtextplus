# Fase 8b-5 — Partículas: Valor Padrão por Tipo — Documento de Design

## 1. Objetivo

Quinta de 7 sub-fases. Cada tipo de partícula (Poeira/Bokeh/Faíscas/
Estrelas) assume, na primeira vez que é selecionado, os valores de
Densidade/Tamanho/Opacidade que o usuário definiu como ideais pra
aquele tipo — sem nunca sobrescrever um ajuste manual já feito.

## 2. Levantamento de conflitos (antes de desenhar)

Os 4 tipos compartilham os MESMOS 4 campos (`particles_density/speed/
size_scale/opacity`), e — diferente do Fundo, onde só 1 campo tinha
conflito — aqui os sliders ficam sempre interagíveis independente do
tipo selecionado (nenhuma ocultação prevista pra esta aba). Conferindo
os valores-alvo do usuário campo a campo:

| Campo | Poeira | Bokeh | Faíscas | Estrelas | Conflito? |
|---|---|---|---|---|---|
| Velocidade | 0.76 | 0.76 | 0.76 | 0.76 | **não** — idêntico nos 4 |
| Densidade | 0.43 | 0.43 | 0.42 | 0.90 | sim |
| Tamanho | 0.80 | 0.81 | 0.72 | 0.70 | sim |
| Opacidade | 0.50 | 0.42 | 0.42 | 0.78 | sim |

`particles_speed` não precisa de tratamento nenhum (mesmo raciocínio
que já livrou `roughness` na Fase 8b-3). Densidade/Tamanho/Opacidade
precisam mesmo de uma flag "customizado" por tipo.

## 3. Arquitetura

**4 campos novos na `Config`** (`int`, `0/1`, padrão `0`):
`particles_dust_customized`, `particles_bokeh_customized`,
`particles_sparks_customized`, `particles_stars_customized`.

**Regra**: ao selecionar um tipo (combo `IDC_PARTKIND`) com a flag
daquele tipo ainda em `0`, aplica os valores de Densidade/Velocidade/
Tamanho/Opacidade daquele tipo (Velocidade sempre 0.76, aplicada junto
por simplicidade — é o mesmo valor de qualquer forma, não custa nada
reaplicá-la), atualiza a posição dos 4 sliders e os rótulos numéricos
na hora. Mexer manualmente em QUALQUER um dos 4 sliders marca a flag
do tipo ATUALMENTE selecionado como customizada — não precisa de
discriminação por `HWND` control-a-control (ao contrário de Efeitos/
Pós, esta aba não hospeda sliders de mais de um "grupo" ao mesmo
tempo, então qualquer slider mexido aqui pertence ao mesmo tipo ativo).

## 4. Valores por tipo (referência)

| Tipo | Densidade | Velocidade | Tamanho | Opacidade |
|---|---|---|---|---|
| Poeira (padrão) | 0.43 | 0.76 | 0.80 | 0.50 |
| Bokeh | 0.43 | 0.76 | 0.81 | 0.42 |
| Faíscas | 0.42 | 0.76 | 0.72 | 0.42 |
| Estrelas | 0.90 | 0.76 | 0.70 | 0.78 |

(Poeira já é o valor de fábrica desde a Fase 8b-1, por ser o tipo
padrão — os outros 3 nunca foram aplicados em lugar nenhum ainda.)

## 5. Testes

Round-trip + clamp dos 4 campos novos em `test_config.c` (mesmo
padrão já usado). Verificação visual real: selecionar cada tipo do
zero (registro limpo) e conferir os 4 sliders/rótulos; depois, mexer
manualmente num slider de um tipo, trocar pra outro tipo e voltar,
confirmando que o valor mexido foi preservado.

## 6. Fora de escopo

Mapa de ambiente embutido (fase 6), presets (fase 7).
