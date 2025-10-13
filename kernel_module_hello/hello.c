#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <generated/utsrelease.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zs");
MODULE_DESCRIPTION("Simple Hello World kernel module");
MODULE_VERSION("1.0");

static int __init hello_init(void)
{
    pr_info("HelloModule: Hello, kernel world! (ver=%s)\n", UTS_RELEASE);
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("HelloModule: Goodbye, kernel world!\n");
}

module_init(hello_init);
module_exit(hello_exit);
