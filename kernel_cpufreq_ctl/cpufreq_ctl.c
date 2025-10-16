#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cpufreq.h>

#define DEVICE_NAME "cpufreq_ctl"
#define CLASS_NAME  "cpufreq"

#define IOCTL_SET_FREQ _IOW('q', 1, struct cpufreq_ioctl_data)

struct cpufreq_ioctl_data {
    unsigned int cpu;
    unsigned int freq;
};

static struct class *cpufreq_class;
static struct cdev cpufreq_cdev;
static dev_t devt;

static long cpufreq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct cpufreq_ioctl_data data;
    struct cpufreq_policy *policy;
    int ret = 0;

    if (cmd != IOCTL_SET_FREQ)
        return -EINVAL;

    if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
        return -EFAULT;

    pr_info("cpufreq_ctl: set CPU%u -> %u kHz\n", data.cpu, data.freq);

    policy = cpufreq_cpu_get(data.cpu);
    if (!policy)
        return -EINVAL;

    ret = cpufreq_driver_target(policy, data.freq, CPUFREQ_RELATION_L);
    cpufreq_cpu_put(policy);

    if (ret)
        pr_err("cpufreq_ctl: failed to set freq, ret=%d\n", ret);
    else
        pr_info("cpufreq_ctl: success\n");

    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = cpufreq_ioctl,
};

static int __init cpufreq_ctl_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&devt, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&cpufreq_cdev, &fops);
    ret = cdev_add(&cpufreq_cdev, devt, 1);
    if (ret)
        goto err_unregister;

    cpufreq_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(cpufreq_class)) {
        ret = PTR_ERR(cpufreq_class);
        goto err_cdev;
    }

    device_create(cpufreq_class, NULL, devt, NULL, DEVICE_NAME);
    pr_info("cpufreq_ctl: module loaded, /dev/%s ready\n", DEVICE_NAME);
    return 0;

err_cdev:
    cdev_del(&cpufreq_cdev);
err_unregister:
    unregister_chrdev_region(devt, 1);
    return ret;
}

static void __exit cpufreq_ctl_exit(void)
{
    device_destroy(cpufreq_class, devt);
    class_destroy(cpufreq_class);
    cdev_del(&cpufreq_cdev);
    unregister_chrdev_region(devt, 1);
    pr_info("cpufreq_ctl: module unloaded\n");
}

module_init(cpufreq_ctl_init);
module_exit(cpufreq_ctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zs");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Simple CPU frequency control via ioctl");
