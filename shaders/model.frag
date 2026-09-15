#version 330 core
in vec3  vWorld;
in vec3  vNrm;
in vec3  vNrmLocal;
in float vSurf;

uniform mat4  uModel;
uniform vec3  uCamPos;
uniform vec3  uBaseColor;
uniform int   uMode;         // 0 classico, 1 metalico, 2 vidro
uniform float uMetalness;    // 0..1
uniform float uRoughness;    // 0..1 - classico, metalico e vidro
uniform vec3  uEmissiveColor;
uniform float uEmissiveAmount;  // 0..1 - classico e vidro (nao metalico)
uniform sampler2D uEnvTex;
uniform int   uHasEnv;       // 0/1
uniform int   uDebugView;    // 0 normal, 1 fresnel, 2 aresta/curvatura, 3 normal RGB, 4 tipo de superficie
uniform float uEdgeBias;     // 0..1 - so' vidro. 0.5 = neutro (curva original). <0.5 pende pra
                             // transparente (mais area lida como "plana"); >0.5 pende pra
                             // opaco/aresta (mais area lida como "aresta")

out vec4  fragColor;               // classico/metalico
layout(location = 1) out vec4  oAccum;       // vidro (WBOIT) - cor*alpha*peso, alpha*peso no canal A
layout(location = 2) out float oRevealLog;   // vidro (WBOIT) - soma de log(1 - alpha)

const float PI = 3.14159265;
const vec3 KEY_DIR  = normalize(vec3(-0.40, -0.75, -0.55));
const vec3 FILL_DIR = normalize(vec3( 0.55,  0.30, -0.45));

vec3 proc_env(vec3 d)
{
    vec3 sky    = mix(vec3(0.10, 0.12, 0.17), vec3(0.50, 0.60, 0.82),
                      clamp(d.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 ground = vec3(0.06, 0.055, 0.05);
    vec3 col = mix(ground, sky, smoothstep(-0.15, 0.15, d.y));
    col += vec3(1.00, 0.96, 0.88)
           * pow(max(dot(d, normalize(vec3( 0.45, 0.55, 0.35))), 0.0), 40.0) * 4.0;
    col += vec3(0.35, 0.45, 0.65)
           * pow(max(dot(d, normalize(vec3(-0.55, 0.15, -0.30))), 0.0), 10.0) * 0.8;
    return col;
}

vec3 sample_env(vec3 d, float rough)
{
    if (uHasEnv == 1) {
        // uma imagem real ja fica mais macia via mip (LOD por rugosidade);
        // nao mistura com o ambiente procedural, que tem tom proprio (azulado)
        // e "lavava" a cor de mapas reais mesmo em rugosidade baixa.
        vec2 uv = vec2(atan(d.z, d.x) / (2.0 * PI) + 0.5,
                       acos(clamp(d.y, -1.0, 1.0)) / PI);
        return textureLod(uEnvTex, uv, rough * 6.0).rgb;
    }
    vec3 e = proc_env(d);
    vec3 ambient = proc_env(vec3(0.0, 1.0, 0.0)) * 0.6 + proc_env(vec3(0.0, -1.0, 0.0)) * 0.4;
    return mix(e, ambient, rough * 0.6);
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// ruido de valor suave (interpola cantos de grade) - um hash cru por fragmento
// vira estatica; isto da manchas coerentes, do tamanho de uma celula da grade.
float value_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// perturba a direcao de reflexo com ruido suave e coerente no espaco do mundo -
// sem isso, faces planas (a maioria de um texto extrudado) refletem um unico
// ponto do ambiente na face inteira, ficando "chapadas". A variacao lembra uma
// chapa martelada em vez de um espelho perfeito - efeito estilizado, nao fisico.
vec3 jitter_reflection(vec3 R, vec3 worldPos, float amount)
{
    vec2 p = worldPos.xy * 0.55 + worldPos.z * 0.3;
    float n1 = value_noise(p) - 0.5;
    float n2 = value_noise(p + vec2(31.7, 11.3)) - 0.5;
    vec2 j = vec2(n1, n2) * amount;
    vec3 up = (abs(R.y) < 0.99) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tx = normalize(cross(up, R));
    vec3 ty = cross(R, tx);
    return normalize(R + tx * j.x + ty * j.y);
}

void main()
{
    vec3 Nl = normalize(vNrmLocal);
    vec3 N = normalize(mat3(uModel) * Nl);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uCamPos - vWorld);
    vec3 R = reflect(-V, N);
    vec3 base = uBaseColor;
    float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 emissive = uEmissiveColor * uEmissiveAmount;

    // mascara de aresta/curvatura CONTINUA: mede o quanto a normal muda de
    // um pixel pro vizinho na tela (dFdx/dFdy), em vez de uma tag discreta
    // da malha (que daria um salto abrupto exatamente na costura entre
    // triangulos - um "wireframe" involuntario). Proximo de zero numa face
    // plana (tampa/parede), sobe suavemente perto de qualquer aresta/vinco/
    // curva do chanfro. Calculada aqui (fora dos ramos de uMode) pra poder
    // ser reaproveitada tanto pelo Vidro quanto pela visualizacao de debug.
    float edgeSignal = length(dFdx(N)) + length(dFdy(N));
    float edgeAmt = smoothstep(0.0, 0.35, edgeSignal);

    // ferramenta de debug visual (botao direito no preview alterna, ver
    // config_dialog.c) - mostra uma etapa/aspecto isolado do calculo do
    // material em vez do resultado final, tipo os modos de visualizacao
    // de material do Blender. So' de sessao, nao afeta o resultado normal.
    if (uDebugView == 1) { fragColor = vec4(vec3(fres), 1.0); return; }
    if (uDebugView == 2) { fragColor = vec4(vec3(edgeAmt), 1.0); return; }
    if (uDebugView == 3) { fragColor = vec4(N * 0.5 + 0.5, 1.0); return; }
    if (uDebugView == 4) {
        vec3 surfColor = vec3(0.15);                          // fallback
        if (vSurf < 0.5)      surfColor = vec3(0.9, 0.2, 0.2); // 0 tampa da frente
        else if (vSurf < 1.5) surfColor = vec3(0.2, 0.9, 0.2); // 1 tampa de tras
        else if (vSurf < 2.5) surfColor = vec3(0.2, 0.4, 0.9); // 2 parede
        else                  surfColor = vec3(0.95, 0.85, 0.1); // 3 chanfro/aresta
        fragColor = vec4(surfColor, 1.0);
        return;
    }

    if (uMode == 1) {                         // metalico
        // o jitter (efeito "metal martelado") agora escala com a
        // aspereza a partir de zero - em 0 (liso) a reflexao do
        // ambiente sai nitida, quase espelhada; antes tinha um piso de
        // 0.9 que nunca deixava a reflexao ficar realmente nitida.
        vec3 Rj   = jitter_reflection(R, vWorld, 1.6 * uRoughness);
        vec3 env  = sample_env(Rj, uRoughness);
        vec3 tint = mix(vec3(1.0), base, uMetalness);
        vec3 col  = env * tint;
        // fresnel e quase constante numa face plana (a normal nao varia sobre a
        // face) - em vez de um "brilho de borda" ele vira um tingimento uniforme
        // que depende so do angulo de visao do momento, afogando a riqueza do
        // reflexo do ambiente (onde a variacao do material realmente mora).
        // Mantido bem discreto por isso - so um toque, nao um teto de 30-40%.
        float fresAmt = min(fres * (0.15 + 0.85 * (1.0 - uRoughness)), 0.10);
        col = mix(col, vec3(1.0), fresAmt);
        vec3 H     = normalize(-KEY_DIR + V);
        vec3 Hj    = jitter_reflection(H, vWorld + vec3(41.0, 7.0, 23.0), 0.6 * uRoughness);
        float spec = pow(max(dot(N, Hj), 0.0), mix(24.0, 220.0, 1.0 - uRoughness));
        col += tint * spec * (0.35 + 0.35 * (1.0 - uRoughness));
        float kd = max(dot(N, -KEY_DIR), 0.0);
        col = mix(col, base * (0.2 + 0.8 * kd), (1.0 - uMetalness) * 0.5);
        if (vSurf > 1.5 && vSurf < 2.5) col *= 0.9;
        fragColor = vec4(col, 1.0);
        return;
    }
    if (uMode == 2) {                         // vidro (passe transparente)
        // As faces internas/de tras nao sao escondidas por fragmento (a
        // extrusao inteira desenha dos dois lados, ver glDisable(GL_CULL_FACE)
        // em scene.c) - a composicao correta entre elas e' resolvida pelo
        // WBOIT (acumulacao ponderada, ver oAccum/oRevealLog mais abaixo),
        // nao por esconder geometria aqui.
        vec3 env  = sample_env(R, uRoughness * 0.5);
        vec3 refr = base * 0.6;
        // vidro liso (aspereza baixa) deveria parecer bem mais espelhado/
        // polido mesmo olhando de frente, nao so nas bordas (fresnel);
        // vidro aspero fica mais opaco/fosco mesmo de frente.
        float m   = clamp(fres + 0.15 + (1.0 - uRoughness) * 0.35, 0.0, 1.0);
        vec3 col  = mix(refr, env, m);
        vec3 H = normalize(-KEY_DIR + V);
        col += vec3(1.0) * pow(max(dot(N, H), 0.0), 120.0);
        col += emissive;
        // WBOIT: em vez de escrever a cor final direto (que dependeria da
        // ordem de desenho das faces - a causa raiz dos artefatos ja
        // encontrados em letras concavas), acumula cor*alpha*peso e o log
        // de (1-alpha) em 2 saidas separadas. O passe de resolucao
        // (wboit_resolve.frag) desfaz isso depois, independente de ordem.
        float a = mix(0.35, 0.95, m);
        // mascara aresta/plano ja calculada no topo (edgeAmt) - reaproveitada
        // aqui: aresta fica mais opaca, face plana mais transparente, nunca
        // batendo em 0% nem 100%. uEdgeBias remapeia a curva por uma
        // potencia (tipo "curvas" de editor de imagem): 0.5 = neutro
        // (expoente 1, sem mudanca); <0.5 empurra mais area pra perto de 0
        // (mais transparente/plano); >0.5 empurra mais area pra perto de 1
        // (mais opaco/aresta).
        float edgeExp = pow(4.0, 1.0 - 2.0 * uEdgeBias);
        float edgeBiased = pow(edgeAmt, edgeExp);
        a = clamp(a + mix(-0.18, 0.12, edgeBiased), 0.12, 0.88);
        float linearDepth = length(uCamPos - vWorld);
        float weight = a * clamp(0.4 / (1e-5 + pow(linearDepth / 8.0, 4.0)), 1e-2, 3000.0);
        oAccum = vec4(col * a * weight, a * weight);
        oRevealLog = log(max(1.0 - a, 1e-4));
        return;
    }

    /* uMode == 0: especular classico */
    float kd = max(dot(N, -KEY_DIR), 0.0);
    vec3 col = base * (0.12 + 0.88 * kd) * vec3(1.00, 0.96, 0.88);
    col += base * max(dot(N, -FILL_DIR), 0.0) * 0.30 * vec3(0.55, 0.62, 0.80);
    vec3 H = normalize(-KEY_DIR + V);
    float specPow  = mix(6.0, 260.0, 1.0 - uRoughness);
    float specTerm = pow(max(dot(N, H), 0.0), specPow);
    col += vec3(1.0) * specTerm * mix(0.12, 0.9, 1.0 - uRoughness);
    col += vec3(0.55, 0.68, 0.95) * pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.35;
    // aspereza baixa = verniz/plastico polido: um leve reflexo do
    // ambiente por cima (como o brilho de uma superficie lisa refletindo
    // o entorno); some conforme a superficie fica mais aspera/fosca.
    float glossAmt = (1.0 - uRoughness);
    if (glossAmt > 0.0) {
        vec3 glossEnv = sample_env(jitter_reflection(R, vWorld, 1.6 * uRoughness), uRoughness);
        col += glossEnv * glossAmt * glossAmt * 0.30;
    }
    col += emissive;
    if (vSurf > 1.5 && vSurf < 2.5) col *= 0.92;
    fragColor = vec4(col, 1.0);
}
