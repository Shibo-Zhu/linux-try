The minnest usages of the sched_cpufreq_kthread module are as follows:
```c
/* 在文件顶部加入 */
#ifdef CONFIG_SCHED_CPUFREQ_SINGLE
extern void sched_check_and_update_cpufreq(struct task_struct *next, int cpu);
#endif

...

/* 在 __schedule() 中 context_switch 返回后增加如下： */
rq = context_switch(rq, prev, next, &rf);

/* === 调度完成后安全点：通知调频模块（可被编译开关控制） === */
#ifdef CONFIG_SCHED_CPUFREQ_SINGLE
    /* pass next and cpu_of(rq) (cpu param unused in module implementation) */
    sched_check_and_update_cpufreq(next, cpu_of(rq));
#endif
```