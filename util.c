#include "dump1090.h"

/* Was file-local `static` in the original monolithic dump1090.c; now
 * shared across modules (aircraft_state.c, main.c), so it's part of the
 * public API declared in dump1090.h. */
long long mstime(void) {
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec)*1000;
    mst += tv.tv_usec/1000;
    return mst;
}

