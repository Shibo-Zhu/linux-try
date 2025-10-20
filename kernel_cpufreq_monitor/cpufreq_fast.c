#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *entry;

static ssize_t read_freq(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    char kbuf[32];
    unsigned int freq = 0;

    struct cpufreq_policy *policy = cpufreq_cpu_get(0); // CPU0
    if (policy) {
        freq = policy->cur;
        cpufreq_cpu_put(policy);
    }

    int n = snprintf(kbuf, sizeof(kbuf), "%u\n", freq);
    return simple_read_from_buffer(buf, len, offset, kbuf, n);
}

static const struct proc_ops fops = {
    .proc_read = read_freq,
};

static int __init cpufreq_fast_init(void)
{
    entry = proc_create("cpufreq_fast", 0444, NULL, &fops);
    pr_info("cpufreq_fast module loaded.\n");
    return 0;
}

static void __exit cpufreq_fast_exit(void)
{
    proc_remove(entry);
    pr_info("cpufreq_fast module unloaded.\n");
}

module_init(cpufreq_fast_init);
module_exit(cpufreq_fast_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zs");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("A simple module to read current CPU frequency from /proc/cpufreq_fast");