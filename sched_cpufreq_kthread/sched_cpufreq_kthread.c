// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/smp.h>
#include <linux/irq_work.h>
#include <linux/atomic.h>
#include <linux/notifier.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zs");
MODULE_DESCRIPTION("RT Task CPUFreq Control via exported notifier + kthread");
MODULE_VERSION("1.0");

/* --- 模块状态 --- */
static unsigned int target_freq = 0;
static atomic_t freq_pending;
static struct irq_work freq_irq_work;
static struct task_struct *freq_kthread;

/* --- 外部内核符号 --- */
extern struct raw_notifier_head cpufreq_task_switch_notifier;

/* --- 调频线程: CPU1 修改 CPU0 --- */
static int freq_thread_fn(void *data)
{
    int cpu_target = 0;
    struct cpufreq_policy *policy;
    unsigned int freq;

    pr_info("freq_thread running on CPU%d (target CPU%d)\n",
            smp_processor_id(), cpu_target);

    while (!kthread_should_stop()) {
        set_current_state(TASK_INTERRUPTIBLE);

        if (!atomic_read(&freq_pending)) {
            schedule();
            continue;
        }

        __set_current_state(TASK_RUNNING);
        atomic_set(&freq_pending, 0);

        freq = target_freq;
        policy = cpufreq_cpu_get(cpu_target);
        if (policy) {
            pr_info("Updating CPU%d freq to %u kHz\n", cpu_target, freq);
            cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);
            cpufreq_cpu_put(policy);
        } else {
            pr_warn("CPU%d: no cpufreq policy found\n", cpu_target);
        }
    }

    pr_info("freq_thread exiting on CPU%d\n", smp_processor_id());
    return 0;
}

/* --- irq_work 回调 --- */
static void freq_irq_work_func(struct irq_work *work)
{
    if (freq_kthread)
        wake_up_process(freq_kthread);
}

/* --- Notifier 回调 --- */
static int cpufreq_task_switch_cb(struct notifier_block *nb,
                                  unsigned long val, void *data)
{
    struct task_struct *next = data;

    /* 仅对实时任务触发 */
    if ((next->policy == SCHED_FIFO || next->policy == SCHED_RR) &&
        next->cpufreq > 0) {
        target_freq = next->cpufreq;
        atomic_set(&freq_pending, 1);
        irq_work_queue(&freq_irq_work);
    }

    return NOTIFY_OK;
}

static struct notifier_block cpufreq_nb = {
    .notifier_call = cpufreq_task_switch_cb,
};

/* --- 模块初始化 --- */
static int __init sched_cpufreq_init(void)
{
    pr_info("Initializing sched_cpufreq_update module (CPU1 -> CPU0)\n");

    atomic_set(&freq_pending, 0);
    init_irq_work(&freq_irq_work, freq_irq_work_func);

    /* 创建线程绑定 CPU1 */
    freq_kthread = kthread_create(freq_thread_fn, NULL, "freq_thread_cpu1");
    if (IS_ERR(freq_kthread)) {
        pr_err("Failed to create freq_thread\n");
        return PTR_ERR(freq_kthread);
    }
    kthread_bind(freq_kthread, 1);
    wake_up_process(freq_kthread);

    /* 注册回调到内核 notifier 链 */
    raw_notifier_chain_register(&cpufreq_task_switch_notifier, &cpufreq_nb);

    return 0;
}

/* --- 模块卸载 --- */
static void __exit sched_cpufreq_exit(void)
{
    if (freq_kthread)
        kthread_stop(freq_kthread);

    /* 注销回调 */
    raw_notifier_chain_unregister(&cpufreq_task_switch_notifier, &cpufreq_nb);

    pr_info("sched_cpufreq_update module unloaded\n");
}

module_init(sched_cpufreq_init);
module_exit(sched_cpufreq_exit);
