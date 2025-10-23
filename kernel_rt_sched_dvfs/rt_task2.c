#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sched.h>

#define PR_SET_CPUFREQ 0x6001
#define CPU_FREQ_2 600000   // 600MHz
#define PERIOD_NS 2000000000ULL
#define WORK_NS   300000000ULL

void busy_work_ns(long ns) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do {
        clock_gettime(CLOCK_MONOTONIC, &now);
    } while ((now.tv_sec - start.tv_sec) * 1e9 + (now.tv_nsec - start.tv_nsec) < ns);
}

int main() {
    struct timespec next;
    FILE *log = fopen("rt_task2.log", "w");
    if (!log) { perror("log open"); return 1; }

    // 绑定 CPU0
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
        perror("sched_setaffinity");

    // 设置实时调度
    struct sched_param param = { .sched_priority = 80 };
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0)
        perror("sched_setscheduler");

    // 设置任务期望 CPU 频率
    if (prctl(PR_SET_CPUFREQ, CPU_FREQ_2, 0, 0, 0) != 0)
        perror("prctl set freq");

    // 初始化下一个周期时间
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (1) {
        struct timespec monotonic_now, realtime_now;
        clock_gettime(CLOCK_MONOTONIC, &monotonic_now);
        clock_gettime(CLOCK_REALTIME, &realtime_now);

        fprintf(log, "Start: MONO %ld.%09ld | REAL %ld.%09ld\n",
                monotonic_now.tv_sec, monotonic_now.tv_nsec,
                realtime_now.tv_sec, realtime_now.tv_nsec);
        fflush(log);

        busy_work_ns(WORK_NS);

        // 计算下一个周期时间
        next.tv_nsec += PERIOD_NS;
        while (next.tv_nsec >= 1000000000) {
            next.tv_nsec -= 1000000000;
            next.tv_sec++;
        }

        // 高精度睡眠到下一个周期
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    fclose(log);
    return 0;
}
