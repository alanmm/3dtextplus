#ifndef M3DT_GEOM_TYPES_H
#define M3DT_GEOM_TYPES_H
#include "util/mathx.h"

typedef struct { v2 *pts; int count; } Contour;   /* fechado (fechamento implicito) */

typedef struct {
    Contour *contours;
    int      count;
    float    minx, miny, maxx, maxy;               /* bbox da uniao */
    int      fill_rule;   /* 0 nonzero (TESS_WINDING_NONZERO), 1 evenodd (TESS_WINDING_ODD) */
} ContourSet;

#endif
