// test_cpufreq.c
#include <stdio.h>
#include <sys/prctl.h>
#include <errno.h>

#define PR_SET_CPUFREQ    0x6001
#define PR_GET_CPUFREQ    0x6002

int main(void) {
    int freq = 800000; // 任意测试值
    int ret;

    // 设置
    ret = prctl(PR_SET_CPUFREQ, freq, 0, 0, 0);
    if (ret != 0) {
        perror("prctl(PR_SET_CPUFREQ) failed");
        return 1;
    }
    printf("Set cpufreq = %d\n", freq);

    // 获取
    int get_freq = -1;
    ret = prctl(PR_GET_CPUFREQ, &get_freq, 0, 0, 0);
    if (ret != 0) {
        perror("prctl(PR_GET_CPUFREQ) failed");
        return 1;
    }
    printf("Got cpufreq = %d\n", get_freq);

    return 0;
}
