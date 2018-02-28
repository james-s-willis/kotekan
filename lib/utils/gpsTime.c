#include "gpsTime.h"
#include "errors.h"

int gps_time_set = 0;
// Value stored in nano seconds.
static uint64_t frame0_time = 0;

void set_global_gps_time(uint64_t frame0) {
    frame0_time = frame0;
    gps_time_set = 1;
}

int is_gps_global_time_set() {
    return gps_time_set;
}

struct timespec compute_gps_time(uint64_t fpga_seq_num) {
    struct timespec ts;
    // The current time is time0 + N * 2.56 ms.
    uint64_t current_time = frame0_time + fpga_seq_num * 2560;
    ts.tv_sec = current_time / 1000000000;
    ts.tv_nsec = current_time % 1000000000;

    return ts;
}