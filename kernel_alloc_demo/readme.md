## Kernel 内存分配示例模块

- 目的：对比三种内核内存分配方式：

    - kmalloc: 小块、内存连续（对 CPU 可见），通常来自 slab/SLUB 缓存（可以在 /proc/slabinfo 找到 kmalloc-*）。

    - alloc_pages: 按页分配，可申请高阶页（order>0），返回 struct page *，物理上连续的页块。

    - vmalloc: 为大块分配虚拟连续、物理不必连续的内存（散布在物理页中并被映射为连续虚拟地址），常用于较大缓冲区。

- 接口：

    - 提供一个 sysfs kobject：/sys/kernel/alloc_demo/，用来设置 mode、size、count 并触发 action（alloc / free）。

    - 提供一个 /proc/alloc_demo（只读），用于显示当前模块统计（成功/失败次数及当前活跃分配）。

- 实现细节：

    - 模块会把每次分配得到的指针保存到内核中的指针数组中（便于 later free），防止内存被立即回收；释放通过 free_all 操作释放。

    - 统计信息使用自建计数器（带自旋/互斥保护）。

- 验证：

    - kmalloc 分配会在 /proc/slabinfo 的 kmalloc-* 池里体现（取决于 size）。

    - alloc_pages 分配会导致内核页分配，可通过 grep /proc/kpageflags（或使用 page 中的 COMPOUND/ORDER 标记）观察高阶页行为（高阶页比单页可见差异）。

    - vmalloc 分配的内存通常在 vmalloc 区域内，物理页分散、不在 kmalloc slab 池里。

    - 对于内存错误检测：若内核启用了 KASAN / kmemleak，可以用它们来检测越界或泄露。

## 代码说明（关键点）：

- sysfs 在 /sys/kernel/alloc_demo/ 下创建 4 个属性：

    - mode：kmalloc|alloc_pages|vmalloc 或 0|1|2。

    - size：分配字节数（size_t）。

    - count：分配次数（int）。

    - action：写入 "alloc" / "free" / "alloc_and_free" 来执行动作。

- /proc/alloc_demo 显示当前活跃记录、计数等信息。

- alloc_rec 保存 ptr 和（对于 alloc_pages）struct page *，便于正确释放。

- get_order(size) 用于 alloc_pages 的阶数计算（确保 (1 << order) * PAGE_SIZE >= size）。

- 所有对共享状态的操作由 mutex 保护。

## 使用方法：
```bash
# 编译
make

# 加载模块（需要 root）
sudo insmod alloc_demo.ko
# or: sudo modprobe alloc_demo (if installed to proper location)

# 查看 sysfs/proc
ls -l /sys/kernel/alloc_demo
cat /proc/alloc_demo

# 卸载
sudo rmmod alloc_demo
```