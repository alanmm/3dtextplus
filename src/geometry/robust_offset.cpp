#include "geometry/robust_offset.h"

#include <clipper2/clipper.core.h>
#include <clipper2/clipper.offset.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace Clipper2Lib;

namespace {

constexpr double SCALE = 1.0e6;   /* coordenadas em unidades de em (tipicamente -2..2) */

double cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

double path_area(const PathD &p)
{
    double a = 0.0;
    size_t n = p.size();
    for (size_t i = 0; i < n; ++i) {
        const PointD &p0 = p[i], &p1 = p[(i + 1) % n];
        a += p0.x * p1.y - p1.x * p0.y;
    }
    return 0.5 * a;
}

/* poligono de offset robusto de `pts` (n pontos) pela distancia com sinal
   `delta` (convencao do Clipper2: positivo cresce um contorno CCW). Contornos
   de fonte podem vir CCW (silhueta externa) ou CW (buraco/contador, ex. o
   miolo do "D"/"O") - sempre normaliza pra CCW antes de chamar o Clipper2
   (revertendo a ordem dos pontos se preciso) em vez de confiar que a
   biblioteca lida bem com entrada CW: offset de poligono assume CCW como
   convencao padrao/mais testada na maioria das libs, e alimentar CW direto
   produzia offsets com topologia errada (looping invertido) especificamente
   nos contornos de buraco - side visivel como triangulos de chanfro com
   normal invertida bem na regiao do contador de letras como "D". */
std::vector<PathD> robust_offset_polygon(const v2 *pts, int n, double delta_mag, bool want_grow)
{
    PathD inputD;
    inputD.reserve((size_t)n);
    for (int i = 0; i < n; ++i) inputD.push_back(PointD((double)pts[i].x, (double)pts[i].y));

    bool wasCW = path_area(inputD) < 0.0;
    if (wasCW) std::reverse(inputD.begin(), inputD.end());

    int err = 0;
    Path64 subject = ScalePath<int64_t, double>(inputD, SCALE, err);

    double delta = want_grow ? delta_mag : -delta_mag;
    ClipperOffset co(2.0 /* miter_limit */);
    co.AddPath(subject, JoinType::Miter, EndType::Polygon);
    Paths64 result64;
    co.Execute(delta * SCALE, result64);
    int err2 = 0;
    PathsD resultD = ScalePaths<double, int64_t>(result64, 1.0 / SCALE, err2);

    return std::vector<PathD>(resultD.begin(), resultD.end());
}

/* menor t>0 tal que (p + dir*t) cruza algum segmento de `R` (soup de
   arestas de 1+ laços), sem passar de `max_t`. Retorna -1 se nenhum
   cruzamento pra frente dentro desse alcance.

   O limite `max_t` e' essencial: sem ele, um raio pode acertar uma aresta
   de R bem distante mas geometricamente NAO relacionada (outra parte do
   mesmo contorno complexo, ex. o outro lado de uma curva do "3"), dando
   um clamp erratico e minusculo nesse vertice - a malha resultante fica
   cheia de vertices individuais colapsados quase no ponto original,
   espalhados pelo contorno (visualmente: fragmentos soltos, especialmente
   no modo Arredondado, que e' muito sensivel a inconsistencia entre
   vertices vizinhos). Limitar a busca a uma vizinhanca plausivel do
   offset alvo evita isso. */
double ray_hit_distance(v2 p, v2 dir, const std::vector<PathD> &R, double max_t)
{
    const double EPS = 1e-7;
    double best = -1.0;
    for (const auto &poly : R) {
        size_t n = poly.size();
        if (n < 2) continue;
        for (size_t k = 0; k < n; ++k) {
            const PointD &a = poly[k];
            const PointD &b = poly[(k + 1) % n];
            double ex = b.x - a.x, ey = b.y - a.y;
            double denom = cross((double)dir.x, (double)dir.y, ex, ey);
            if (std::fabs(denom) < 1e-12) continue;
            double rx = a.x - (double)p.x, ry = a.y - (double)p.y;
            double t = cross(rx, ry, ex, ey) / denom;
            double u = cross(rx, ry, (double)dir.x, (double)dir.y) / denom;
            if (t > EPS && t <= max_t && u >= -1e-9 && u <= 1.0 + 1e-9) {
                if (best < 0.0 || t < best) best = t;
            }
        }
    }
    return best;
}

/* mesma bissetriz/passo "ingenuo" da inset_contour() original (piso de
   cosseno 0.3 incluso) - usado como candidato inicial e como ultimo
   recurso quando o raio nao cruza o poligono robusto (caso degenerado). */
struct NaiveStep { v2 p, bis; float step; bool skip; };

NaiveStep naive_step_at(const v2 *pts, int n, int i, float d, int inward_left)
{
    NaiveStep r{};
    v2 pm = pts[(i - 1 + n) % n];
    v2 p  = pts[i];
    v2 pp = pts[(i + 1) % n];
    r.p = p;

    v2 e0 = { p.x - pm.x, p.y - pm.y };
    v2 e1 = { pp.x - p.x, pp.y - p.y };
    float l0 = sqrtf(e0.x * e0.x + e0.y * e0.y);
    float l1 = sqrtf(e1.x * e1.x + e1.y * e1.y);
    /* so' desiste se as DUAS arestas vizinhas forem degeneradas (vertice
       isolado de verdade). Uma unica aresta zerada e' um caso comum e
       inofensivo: todo contorno de fonte tem um ponto de "fechamento"
       duplicado no wraparound (ultimo ponto == primeiro) - tratar isso
       como "sem corner, so' continua na direcao da aresta valida" em vez
       de pular o vertice inteiro evita um pinçamento pontual bem ali. */
    if (l0 < 1e-9f && l1 < 1e-9f) { r.skip = true; return r; }
    bool haveN0 = l0 >= 1e-9f, haveN1 = l1 >= 1e-9f;
    if (haveN0) { e0.x /= l0; e0.y /= l0; }
    if (haveN1) { e1.x /= l1; e1.y /= l1; }

    v2 n0 = {0,0}, n1 = {0,0};
    if (haveN0) n0 = inward_left ? (v2){ -e0.y, e0.x } : (v2){ e0.y, -e0.x };
    if (haveN1) n1 = inward_left ? (v2){ -e1.y, e1.x } : (v2){ e1.y, -e1.x };
    if (!haveN0) n0 = n1;
    if (!haveN1) n1 = n0;

    v2 bis = { n0.x + n1.x, n0.y + n1.y };
    float bl = sqrtf(bis.x * bis.x + bis.y * bis.y);
    if (bl < 1e-4f) { r.skip = true; return r; }
    bis.x /= bl; bis.y /= bl;

    float cosang = bis.x * n0.x + bis.y * n0.y;
    r.bis = bis;
    r.step = d / (cosang > 0.3f ? cosang : 0.3f);
    return r;
}

} // namespace

extern "C" void robust_offset_contour(const Contour *c, float d, int inward_left, v2 *out)
{
    int n = c->count;
    bool want_grow = (d < 0.0f);   /* d<0 cresce pra fora (bevel outset); d>0 encolhe (parede oca) */
    std::vector<PathD> R = robust_offset_polygon(c->pts, n, std::fabs((double)d), want_grow);

    for (int i = 0; i < n; ++i) {
        NaiveStep ns = naive_step_at(c->pts, n, i, d, inward_left);
        if (ns.skip) { out[i] = ns.p; continue; }

        v2 raydir = (ns.step >= 0.0f) ? ns.bis : (v2){ -ns.bis.x, -ns.bis.y };
        double naiveMag = std::fabs((double)ns.step);
        double max_search = std::fabs((double)d) * 8.0 + 1e-4;
        double hit = ray_hit_distance(ns.p, raydir, R, max_search);
        double mag = (hit >= 0.0) ? std::min(naiveMag, hit * 0.98) : naiveMag;

        out[i] = (v2){ ns.p.x + raydir.x * (float)mag, ns.p.y + raydir.y * (float)mag };
    }
}
