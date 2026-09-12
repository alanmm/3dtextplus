#ifndef M3DT_CLOCKFMT_H
#define M3DT_CLOCKFMT_H
#include <windows.h>

/* Formata st em UTF-8 no formato localizado do Windows (locale do
   usuario). show_date prefixa a data (GetDateFormatEx) seguida de '\n'
   antes da hora; show_seconds inclui segundos na hora (senao usa
   TIME_NOSECONDS). Nunca deixa out vazio nem estoura outsz. */
void clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz);

#endif
