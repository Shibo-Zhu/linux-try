#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define PROC_PATH "/proc/cpufreq_fast"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <interval_ms> <log_path>\n", argv[0]);
        return 1;
    }

    int interval = atoi(argv[1]);
    const char *log_path = argv[2];

    FILE *log = fopen(log_path, "w");
    if (!log) {
        perror("fopen");
        return 1;
    }

    struct timespec ts;
    while (1) {
        FILE *f = fopen(PROC_PATH, "r");
        if (!f) {
            perror("open /proc/cpufreq_fast");
            break;
        }

        int freq = 0;
        fscanf(f, "%d", &freq);
        fclose(f);

        clock_gettime(CLOCK_REALTIME, &ts);
        fprintf(log, "%ld.%03ld %d\n", ts.tv_sec, ts.tv_nsec / 1000000, freq);
        fflush(log);

        usleep(interval * 1000);
    }

    fclose(log);
    return 0;
}
