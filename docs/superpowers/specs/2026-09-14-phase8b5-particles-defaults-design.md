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

> **Correção (pós-implementação):** o desenho original abaixo (4 flags
> booleanas `*_customized` apontando pra um ÚNICO slot compartilhado
> `particles_density/size_scale/opacity`) foi testado com captura real e
> **falhou**: como o slot é compartilhado entre os 4 tipos, ao voltar
> pra um tipo já "customizado" a tela mostrava os valores que sobraram
> do ÚLTIMO tipo visitado, não o ajuste manual daquele tipo (ex.:
> editei Densidade da Poeira pra 0.20, visitei Bokeh, voltei pra Poeira
> — mostrou 0.43/0.81/0.42, os valores do Bokeh). Isso viola a promessa
> deste documento ("sem nunca sobrescrever um ajuste manual já feito").
> Diferente do Fundo (fase 8b-4), onde só 2 estados compartilham 1 slot
> e a limitação foi documentada e aceita, aqui são 4 tipos — matemat­i­
> camente impossível preservar 4 valores independentes num slot só.
> **Correção aplicada:** substituídas as 4 flags por **12 campos**
> (Densidade/Tamanho/Opacidade × 4 tipos), cada tipo com memória
> própria. Ver seção 3-bis abaixo. Velocidade continua sem tratamento
> (sem conflito, 0.76 em todos).

**(Desenho original, substituído — mantido como registro):** 4 campos
`int` (`0/1`) `particles_dust_customized` etc., reaplicando o padrão do
tipo só na primeira vez que sua flag estivesse zerada.

## 3-bis. Arquitetura corrigida

**12 campos novos na `Config`** (`float`), um trio (Densidade/Tamanho/
Opacidade) por tipo: `particles_dust_density/size/opacity`,
`particles_bokeh_density/size/opacity`,
`particles_sparks_density/size/opacity`,
`particles_stars_density/size/opacity`. `config_defaults()` semeia cada
trio com o valor-alvo do próprio tipo (tabela da seção 4).

**Regra**: ao selecionar um tipo (combo `IDC_PARTKIND`), os valores
"atuais" (`particles_density/size_scale/opacity`, os mesmos já lidos
pelo renderer) são carregados a partir do trio de memória daquele
tipo — sempre, incondicionalmente, sem precisar checar se é a primeira
vez. Mexer em qualquer um dos 4 sliders grava o novo valor tanto nos
campos "atuais" quanto no trio de memória do tipo ATUALMENTE
selecionado (sem discriminação por `HWND`, mesmo raciocínio do desenho
original — só um tipo ativo por vez nesta aba). Velocidade permanece
um campo único compartilhado, sem memória por tipo.

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
