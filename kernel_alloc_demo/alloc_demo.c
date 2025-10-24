// alloc_demo.c
// Build: use provided Makefile
// Purpose: Demonstrate kmalloc / alloc_pages / vmalloc, provide sysfs control and proc output.

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>        // kmalloc, kfree
#include <linux/vmalloc.h>     // vmalloc, vfree
#include <linux/mm.h>          // struct page, alloc_pages, __free_pages, page_address
#include <linux/gfp.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/version.h>     // for LINUX_VERSION_CODE

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zs");
MODULE_DESCRIPTION("Demo module: kmalloc / alloc_pages / vmalloc differences (sysfs + procfs)");
MODULE_VERSION("1.0");

/* ---------- configuration and state ---------- */

enum alloc_mode {
    MODE_KMALLOC = 0,
    MODE_ALLOC_PAGES = 1,
    MODE_VMALLOC = 2,
};

static const char *mode_names[] = { "kmalloc", "alloc_pages", "vmalloc" };

/* Controlled via sysfs: mode,size,count,action */
static int cur_mode = MODE_KMALLOC;
static size_t cur_size = 4096;   /* default 4 KiB */
static int cur_count = 1;        /* how many allocations to perform */

struct alloc_rec {
    void *ptr;          /* pointer returned to us (vaddr or page_address)：内核内存分配接口返回给我们的实际虚拟地址 */
    struct page *page;  /* only used when alloc_pages returns page */
    size_t size;        /* requested size */
    int mode;
};

struct alloc_demo_state {
    struct alloc_rec *recs; /* array of length alloc_capacity */
    int alloc_capacity;
    int alloc_used;
    unsigned long success_count;
    unsigned long fail_count;
    struct mutex lock;
} state;

/* sysfs kobject */
static struct kobject *alloc_kobj;

/* proc entry name */
#define PROC_NAME "alloc_demo"

/* ---------- utilities ---------- */

static void free_record(struct alloc_rec *r)
{
    if (!r || !r->ptr)
        return;

    switch (r->mode) {
    case MODE_KMALLOC:
        kfree(r->ptr);
        break;
    case MODE_VMALLOC:
        vfree(r->ptr);
        break;
    case MODE_ALLOC_PAGES:
        if (r->page) {
            int order = get_order(r->size);
            __free_pages(r->page, order);
        }
        break;
    default:
        break;
    }

    r->ptr = NULL;
    r->page = NULL;
}

/* Called from sysfs action=alloc */
static void do_allocs(int mode, size_t size, int count)
{
    int i;
    mutex_lock(&state.lock);

    /* ensure capacity */
    if (count + state.alloc_used > state.alloc_capacity) {
        int newcap = max(state.alloc_capacity * 2, state.alloc_used + count);
        struct alloc_rec *newr = krealloc(state.recs, sizeof(struct alloc_rec) * newcap, GFP_KERNEL);
        if (!newr) {
            pr_err("alloc_demo: failed to grow rec array\n");
            mutex_unlock(&state.lock);
            return;
        }
        memset(newr + state.alloc_capacity, 0, sizeof(struct alloc_rec) * (newcap - state.alloc_capacity));
        state.recs = newr;
        state.alloc_capacity = newcap;
    }

    for (i = 0; i < count; i++) {
        void *ptr = NULL;
        struct page *pg = NULL;
        int order;
        size_t allocate_size = size;

        switch (mode) {
        case MODE_KMALLOC:
            ptr = kmalloc(allocate_size, GFP_KERNEL);
            break;
        case MODE_VMALLOC:
            ptr = vmalloc(allocate_size);
            break;
        case MODE_ALLOC_PAGES:
            order = get_order(allocate_size);
            pg = alloc_pages(GFP_KERNEL, order);
            if (pg)
                ptr = page_address(pg);
            break;
        default:
            break;
        }

        if (ptr) {
            struct alloc_rec *r = &state.recs[state.alloc_used++];
            r->ptr = ptr;
            r->page = pg;
            r->size = allocate_size;
            r->mode = mode;
            state.success_count++;
        } else {
            state.fail_count++;
            pr_warn("alloc_demo: allocation failed mode=%d size=%zu idx=%d\n", mode, allocate_size, i);
        }
    }

    mutex_unlock(&state.lock);
}

/* Free all stored allocations */
static void free_all_allocs(void)
{
    int i;
    mutex_lock(&state.lock);
    for (i = 0; i < state.alloc_used; i++) {
        free_record(&state.recs[i]);
    }
    state.alloc_used = 0;
    mutex_unlock(&state.lock);
}

/* ---------- procfs output ---------- */

static int proc_show(struct seq_file *m, void *v)
{
    int i;
    seq_printf(m, "alloc_demo module stats\n");
    seq_printf(m, "=======================\n");
    seq_printf(m, "mode (current): %s (%d)\n",
               (cur_mode >= 0 && cur_mode <= MODE_VMALLOC) ? mode_names[cur_mode] : "unknown",
               cur_mode);
    seq_printf(m, "size (current): %zu\n", cur_size);
    seq_printf(m, "count (current): %d\n", cur_count);

    mutex_lock(&state.lock);
    seq_printf(m, "alloc_capacity: %d\n", state.alloc_capacity);
    seq_printf(m, "active_allocs: %d\n", state.alloc_used);
    seq_printf(m, "success_count: %lu\n", state.success_count);
    seq_printf(m, "fail_count: %lu\n", state.fail_count);
    seq_printf(m, "\nactive allocation records:\n");
    for (i = 0; i < state.alloc_used; i++) {
        struct alloc_rec *r = &state.recs[i];
        seq_printf(m, "  [%2d] mode=%s size=%6zu ptr=%p page=%p\n",
                   i,
                   (r->mode >= 0 && r->mode <= MODE_VMALLOC) ? mode_names[r->mode] : "unknown",
                   r->size, r->ptr, r->page);
    }
    mutex_unlock(&state.lock);
    seq_printf(m, "---- end ----\n");
    return 0;
}

static int proc_open_fn(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

/* 兼容 Linux 5.6+ 的接口 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops proc_fops = {
    .proc_open    = proc_open_fn,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations proc_fops = {
    .owner   = THIS_MODULE,
    .open    = proc_open_fn,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
#endif

/* ---------- sysfs handlers ---------- */

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", cur_mode);
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (sysfs_streq(buf, "kmalloc") || sysfs_streq(buf, "0")) {
        cur_mode = MODE_KMALLOC;
    } else if (sysfs_streq(buf, "alloc_pages") || sysfs_streq(buf, "1")) {
        cur_mode = MODE_ALLOC_PAGES;
    } else if (sysfs_streq(buf, "vmalloc") || sysfs_streq(buf, "2")) {
        cur_mode = MODE_VMALLOC;
    } else {
        pr_warn("alloc_demo: unknown mode write: %.*s\n", (int)min(count, (size_t)64), buf);
        return -EINVAL;
    }
    return count;
}

static ssize_t size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%zu\n", cur_size);
}

static ssize_t size_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    unsigned long val;
    if (kstrtoul(buf, 0, &val))
        return -EINVAL;
    cur_size = (size_t)val;
    return count;
}

static ssize_t count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", cur_count);
}

static ssize_t count_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    long val;
    if (kstrtol(buf, 0, &val))
        return -EINVAL;
    if (val <= 0)
        return -EINVAL;
    cur_count = (int)val;
    return count;
}

static ssize_t action_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    if (sysfs_streq(buf, "alloc")) {
        do_allocs(cur_mode, cur_size, cur_count);
    } else if (sysfs_streq(buf, "free")) {
        free_all_allocs();
    } else if (sysfs_streq(buf, "alloc_and_free")) {
        do_allocs(cur_mode, cur_size, cur_count);
        free_all_allocs();
    } else {
        pr_warn("alloc_demo: unknown action '%.*s'\n", (int)min(count, (size_t)64), buf);
        return -EINVAL;
    }
    return count;
}

static struct kobj_attribute mode_attr = __ATTR(mode, 0664, mode_show, mode_store);
static struct kobj_attribute size_attr = __ATTR(size, 0664, size_show, size_store);
static struct kobj_attribute count_attr = __ATTR(count, 0664, count_show, count_store);
static struct kobj_attribute action_attr = __ATTR_WO(action);

static struct attribute *alloc_attrs[] = {
    &mode_attr.attr,
    &size_attr.attr,
    &count_attr.attr,
    &action_attr.attr,
    NULL,
};

static struct attribute_group alloc_attr_group = {
    .attrs = alloc_attrs,
};

/* ---------- module init / exit ---------- */

static int __init alloc_demo_init(void)
{
    int ret;

    pr_info("alloc_demo: init\n");

    mutex_init(&state.lock);
    state.alloc_capacity = 16;
    state.alloc_used = 0;
    state.recs = kcalloc(state.alloc_capacity, sizeof(struct alloc_rec), GFP_KERNEL);
    state.success_count = 0;
    state.fail_count = 0;

    alloc_kobj = kobject_create_and_add("alloc_demo", kernel_kobj);
    if (!alloc_kobj) {
        pr_err("alloc_demo: failed to create kobject\n");
        ret = -ENOMEM;
        goto out_free;
    }

    ret = sysfs_create_group(alloc_kobj, &alloc_attr_group);
    if (ret) {
        pr_err("alloc_demo: sysfs_create_group failed: %d\n", ret);
        kobject_put(alloc_kobj);
        goto out_free;
    }

    if (!proc_create(PROC_NAME, 0444, NULL, &proc_fops)) {
        pr_err("alloc_demo: proc_create failed\n");
        sysfs_remove_group(alloc_kobj, &alloc_attr_group);
        kobject_put(alloc_kobj);
        ret = -ENOMEM;
        goto out_free;
    }

    pr_info("alloc_demo: sysfs at /sys/kernel/alloc_demo, proc at /proc/%s\n", PROC_NAME);
    return 0;

out_free:
    kfree(state.recs);
    return ret;
}

static void __exit alloc_demo_exit(void)
{
    pr_info("alloc_demo: exit - freeing all\n");
    free_all_allocs();

    remove_proc_entry(PROC_NAME, NULL);
    sysfs_remove_group(alloc_kobj, &alloc_attr_group);
    kobject_put(alloc_kobj);

    kfree(state.recs);
    pr_info("alloc_demo: module unloaded\n");
}

module_init(alloc_demo_init);
module_exit(alloc_demo_exit);
