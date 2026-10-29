#include <wasmenv.h>
#include <time.h>

time_t time(time_t *tloc)
{
    time_t t = (time_t) (wasmenv_time_ms() / 1000.0);
    if (tloc)
        *tloc = t;
    return t;
}
