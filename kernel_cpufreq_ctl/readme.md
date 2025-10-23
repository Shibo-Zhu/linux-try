This is a simple linux module to control the CPU frequency scaling driver on Linux systems.

1. Build the module
    ```bash
    chmod +x build.sh
    ./build.sh
    ```

2. Load the module
    ```bash
    sudo insmod cpufreq_ctl.ko
    ```

3. Build the user-space application
    ```bash
    gcc -o cpufreq_ctl cpufreq_ctl.c
    ```

4. Run the user-space application
    ```bash
    sudo ./cpufreq_ctl 0 1100000
    ```
    This command sets the CPU frequency of CPU 0 to 1.1 GHz (1100000 kHz).

5. Validate the change
    ```bash
    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
    ```
    This command should show the current frequency of CPU 0, which should be close(equal) to 1100000 kHz.

6. Unload the module
    ```bash
    sudo rmmod kernel_cpufreq_ctl
    ```