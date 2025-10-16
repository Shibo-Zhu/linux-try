#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/cpufreq_ctl"

#define IOCTL_SET_FREQ _IOW('q', 1, struct cpufreq_ioctl_data)

struct cpufreq_ioctl_data {
    unsigned int cpu;
    unsigned int freq;
};

int main(int argc, char *argv[])
{
    int fd;
    struct cpufreq_ioctl_data data;

    if (argc != 3) {
        fprintf(stderr, "Usage: sudo %s <cpu_id> <freq_khz>\n", argv[0]);
        return 1;
    }

    data.cpu = atoi(argv[1]);
    data.freq = atoi(argv[2]);

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, IOCTL_SET_FREQ, &data) < 0) {
        perror("ioctl");
        close(fd);
        return 1;
    }

    printf("CPU%u frequency set to %u kHz successfully.\n", data.cpu, data.freq);

    close(fd);
    return 0;
}
