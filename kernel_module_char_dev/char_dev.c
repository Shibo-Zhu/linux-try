#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>       // 包含文件操作相关的结构和函数
#include <linux/cdev.h>     // 包含 cdev 结构和相关函数
#include <linux/device.h>   // 包含 class 和 device_create 等函数
#include <linux/uaccess.h>  // 包含 copy_to_user 和 copy_from_user

#define DEVICE_NAME "char_dev"
#define CLASS_NAME  "char_class"
#define MAX_BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device kernel module");
MODULE_VERSION("1.1");

// --- 全局变量 ---
static int    major_number;                 // 存储我们的设备主号
static char   kernel_buffer[MAX_BUFFER_SIZE] = {0}; // 用于存储数据的内核缓冲区
static int    bytes_stored = 0;             // 当前存储在缓冲区中的字节数
static struct class* char_class  = NULL;   // 设备类
static struct device* char_device = NULL;   // 设备实例
static struct cdev    my_cdev;              // 字符设备结构

// --- 文件操作函数声明 ---
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

// 将文件操作与我们的函数关联起来
static struct file_operations fops = {
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .release = dev_release,
    .owner   = THIS_MODULE,
};

// --- 模块初始化函数 ---
static int __init char_dev_init(void) {
    pr_info("CharDevModule: Initializing the character device...\n");

    // 1. 动态分配一个主设备号
    // alloc_chrdev_region(dev_t* dev, unsigned int firstminor, unsigned int count, const char* name)
    if (alloc_chrdev_region(&major_number, 0, 1, DEVICE_NAME) < 0) {
        pr_err("CharDevModule: Failed to allocate a major number\n");
        return -1;
    }
    // MAJOR(major_number) 宏可以从 dev_t 中提取主设备号
    pr_info("CharDevModule: Registered correctly with major number %d\n", MAJOR(major_number));

    // 2. 初始化 cdev 结构，并与文件操作关联
    // void cdev_init(struct cdev *cdev, const struct file_operations *fops)
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    // 3. 将 cdev 添加到内核
    // int cdev_add(struct cdev *p, dev_t dev, unsigned int count)
    if (cdev_add(&my_cdev, major_number, 1) < 0) {
        pr_err("CharDevModule: Failed to add the cdev to the kernel\n");
        // 如果失败，需要释放已分配的设备号
        unregister_chrdev_region(major_number, 1);
        return -1;
    }

    // 4. 创建设备类 (class)
    // struct class *class_create(struct module *owner, const char *name)
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class)) {
        pr_err("CharDevModule: Failed to create the struct class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(major_number, 1);
        return PTR_ERR(char_class);
    }
    pr_info("CharDevModule: Device class created successfully.\n");

    // 5. 在 /dev/ 目录下创建设备文件
    // struct device *device_create(struct class *class, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...)
    char_device = device_create(char_class, NULL, major_number, NULL, DEVICE_NAME);
    if (IS_ERR(char_device)) {
        pr_err("CharDevModule: Failed to create the device\n");
        class_destroy(char_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(major_number, 1);
        return PTR_ERR(char_device);
    }
    pr_info("CharDevModule: Device created successfully at /dev/%s\n", DEVICE_NAME);
    return 0;
}

// --- 模块退出函数 ---
static void __exit char_dev_exit(void) {
    // 按初始化的逆序进行清理
    device_destroy(char_class, major_number); // 5. 销毁设备文件
    class_destroy(char_class);                // 4. 销毁设备类
    cdev_del(&my_cdev);                       // 3. 从内核移除 cdev
    unregister_chrdev_region(major_number, 1);// 1. 释放主设备号

    pr_info("CharDevModule: Goodbye!\n");
}

// --- 文件操作函数实现 ---

static int dev_open(struct inode *inodep, struct file *filep) {
    pr_info("CharDevModule: Device has been opened.\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    pr_info("CharDevModule: Device successfully closed.\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *user_buffer, size_t len, loff_t *offset) {
    int bytes_to_read;

    // 如果 offset 超出我们存储的数据末尾，则没有数据可读 (EOF)
    if (*offset >= bytes_stored) {
        return 0;
    }
    
    // 确定实际可以读取的字节数
    bytes_to_read = len > (bytes_stored - *offset) ? (bytes_stored - *offset) : len;
    if (bytes_to_read == 0) {
        return 0; // 没有更多数据了
    }

    // 将内核数据安全地复制到用户空间
    // copy_to_user(void __user *to, const void *from, unsigned long n)
    if (copy_to_user(user_buffer, kernel_buffer + *offset, bytes_to_read) != 0) {
        return -EFAULT; // 地址错误
    }

    // 更新文件偏移量
    *offset += bytes_to_read;
    
    pr_info("CharDevModule: Sent %d characters to the user\n", bytes_to_read);
    return bytes_to_read; // 返回实际读取的字节数
}

static ssize_t dev_write(struct file *filep, const char *user_buffer, size_t len, loff_t *offset) {
    // 确定可以写入的字节数，防止缓冲区溢出
    int bytes_to_write = len > MAX_BUFFER_SIZE ? MAX_BUFFER_SIZE : len;

    // 将用户空间数据安全地复制到内核空间
    // copy_from_user(void *to, const void __user *from, unsigned long n)
    if (copy_from_user(kernel_buffer, user_buffer, bytes_to_write) != 0) {
        return -EFAULT;
    }

    // 更新已存储的字节数
    bytes_stored = bytes_to_write;

    // 清理字符串末尾的换行符（如果存在），让日志更美观
    if(bytes_stored > 0 && kernel_buffer[bytes_stored-1] == '\n') {
       kernel_buffer[bytes_stored-1] = '\0';
       bytes_stored--;
    }

    pr_info("CharDevModule: Received %d characters from user: '%s'\n", bytes_to_write, kernel_buffer);
    return bytes_to_write; // 返回实际写入的字节数
}

module_init(char_dev_init);
module_exit(char_dev_exit);