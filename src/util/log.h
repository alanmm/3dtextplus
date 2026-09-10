#ifndef M3DT_LOG_H
#define M3DT_LOG_H

void log_init(void);
void log_shutdown(void);
void log_infof(const char *fmt, ...);
void log_errorf(const char *fmt, ...);

/* Puro / testavel: escreve "YYYY-MM-DD HH:MM:SS [LVL] MSG\n" em buf. */
void log_format_line(char *buf, int n, const char *lvl, const char *msg,
                     int Y, int Mo, int D, int h, int mi, int s);

#endif
