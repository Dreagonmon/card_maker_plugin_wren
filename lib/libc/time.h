
#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#define CLOCKS_PER_SEC  ((long long) 1000000)

typedef long long time_t;

long long clock(void);
time_t time(time_t *tloc);

#ifdef __cplusplus
}
#endif

#endif
