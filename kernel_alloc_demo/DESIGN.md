**struct alloc_rec：单次分配记录**

* `void *ptr`：分配得到的内核虚拟地址指针，来自 `kmalloc()`、`vmalloc()` 或 `page_address()`。
* `struct page *page`：仅当通过 `alloc_pages()` 分配时使用，用于保存返回的页结构体指针。
* `size_t size`：记录用户请求分配的字节数（仅对 `kmalloc`、`vmalloc` 有意义）。
* `int mode`：记录分配方式（如 0=kmalloc，1=vmalloc，2=alloc_pages）。

**struct alloc_demo_state：模块全局状态**

* `struct alloc_rec *recs`：用于保存所有已分配记录的数组指针。
* `int alloc_capacity`：`recs` 数组的最大容量。
* `int alloc_used`：当前已使用的记录数。
* `unsigned long success_count`：成功分配次数计数。
* `unsigned long fail_count`：分配失败次数计数。
* `struct mutex lock`：互斥锁，用于在多线程访问时保护上述状态数据。

**sysfs 对象**

* `static struct kobject *alloc_kobj`：在 `/sys/kernel/alloc_demo/` 下创建的 kobject，用于暴露 sysfs 接口（例如触发分配或释放操作）。



**`kobject` 的作用**

1. **统一对象管理**

   * 所有需要在内核中表示、引用、计数、导出到用户空间的对象，都可以用 `kobject` 进行抽象。
   * 它提供了标准化的引用计数机制、层级结构（父子关系）、名字管理。

2. **与 sysfs 的关联**

   * `kobject` 是 `sysfs` 的核心基础：
     当你在 `/sys/` 下看到的目录（如 `/sys/class/net/eth0`）其实就是一个个 `kobject`。
   * 通过 `kobject_create_and_add()` 创建的对象，会自动在 `/sys/kernel/` 等位置生成对应目录。

3. **`kobject` 的典型结构定义**

```c
struct kobject {
    const char        *name;       // 对象名称，对应 sysfs 目录名
    struct list_head  entry;       // 链接到同一父对象的子对象链表
    struct kobject    *parent;     // 父对象，用于形成层级结构
    struct kset       *kset;       // 对象所属的集合
    struct kobj_type  *ktype;      // 对象类型定义（包含release方法）
    struct kref       kref;        // 引用计数
    unsigned int      state_initialized:1;
    unsigned int      state_in_sysfs:1;
    unsigned int      state_add_uevent_sent:1;
    unsigned int      state_remove_uevent_sent:1;
    struct delayed_work release;
};
```

4. **创建kobject属性**

```c
static struct kobj_attribute mode_attr = __ATTR(mode, 0664, mode_show, mode_store);
static struct kobj_attribute size_attr = __ATTR(size, 0664, size_show, size_store);
static struct kobj_attribute count_attr = __ATTR(count, 0664, count_show, count_store);
static struct kobj_attribute action_attr = __ATTR_WO(action);
```
- 每个 `kobj_attribute` 对应 一个 sysfs 文件：

   - `mode_attr` → `/sys/kernel/alloc_demo/mode`

   - `size_attr` → `/sys/kernel/alloc_demo/size`

   - `count_attr` → `/sys/kernel/alloc_demo/count`

   - `action_attr` → `/sys/kernel/alloc_demo/action（只写）`

- 0664 是文件权限（用户读写，组读写，其他只读），__ATTR_WO 是只写文件。

- mode_show / mode_store 是读取\写入文件时调用的函数，定义了 文件读/写时的行为（例如读取当前模式或写入新的分配模式）。

5. **属性数组**

```c
static struct attribute *alloc_attrs[] = {
    &mode_attr.attr,
    &size_attr.attr,
    &count_attr.attr,
    &action_attr.attr,
    NULL,
};
```

- alloc_attrs 是一个 指针数组，列出了要创建的所有属性文件。
- NULL 结尾表示数组结束。


6. **属性组**

```c
static struct attribute_group alloc_attr_group = {
    .attrs = alloc_attrs,
};
```

- alloc_attr_group 把这些属性文件组合成 一个组，便于一次性创建。


7. **创建属性组到 kobject**

```c
ret = sysfs_create_group(alloc_kobj, &alloc_attr_group);
```

- 将 alloc_attr_group 下的所有属性文件挂载到 alloc_kobj（即 /sys/kernel/alloc_demo/ 目录）。

- 系统会在 /sys/kernel/alloc_demo/ 下生成四个文件：
   - mode
   - size
   - count
   - action


**kcalloc分配内存**

```c
state.recs = kcalloc(state.alloc_capacity, sizeof(struct alloc_rec), GFP_KERNEL);
```

简要解释如下：

* **`kcalloc()`**：内核内存分配函数，类似于 `calloc()`，会分配并清零内存。
* **`state.alloc_capacity`**：要分配的元素个数。
* **`sizeof(struct alloc_rec)`**：每个元素的大小。
* **`GFP_KERNEL`**：表示在内核态、可睡眠上下文中分配普通内存。
* **作用**：为 `state.recs` 分配一个可存放 `alloc_capacity` 个 `alloc_rec` 结构体的数组，并初始化为 0。


**kobject_put(alloc_kobj)**

- 释放 kobject 的引用计数（引用数减一），当引用计数降为 0 时，会自动销毁该 kobject 并释放相关资源。

- 内核中的 kobject 使用 引用计数机制 管理生命周期：

   - 每创建或使用一次，会通过 kobject_get() 增加引用计数。

   - 当不再需要时，通过 kobject_put() 减少引用计数。

   - 当引用计数为 0 时，kobject 会被自动释放（包括名字、sysfs 目录等）。


**`proc_create()`**

- 在 `/proc` 文件系统中创建一个新的虚拟文件。  
  - `PROC_NAME` → `"alloc_demo"`，即 `/proc/alloc_demo`；  
  - `0444` → 文件权限（只读）；  
  - `NULL` → 父目录为 `/proc` 根目录；  
  - `&proc_fops` → 文件操作函数表，定义 `/proc/alloc_demo` 的读操作逻辑（此模块中由 `seq_file` 提供）。  

- 若创建失败，`proc_create()` 返回 `NULL`，因此进入 `if` 块。


**sysfs_remove_group(alloc_kobj, &alloc_attr_group)**

- 清理已经创建的 sysfs 属性组 `/sys/kernel/alloc_demo/*`。  
- 防止出现 “sysfs 已创建但 /proc 创建失败” 导致的资源泄漏。

**krealloc()**

- krealloc() 返回的是一块新的内核内存地址（void * 指针）。

- 它是对 kmalloc() / kfree() 的一种包装，用于在堆上重新分配一块连续的内核内存。

- `krealloc(state.recs, sizeof(struct alloc_rec) * newcap, GFP_KERNEL);`内存扩容，会迁移（拷贝）旧数据到新内存中

   1. 第一次分配（state.recs == NULL），直接分配新内存，不涉及迁移。
   2. new_size > 旧容量（扩容）
      - 分配新内存；

      - 把旧内容用 memcpy() 拷贝过去；

      - 释放旧内存；

      - 返回新指针
   3. new_size ≤ 旧容量（不需要扩容），不会重新分配，直接返回原指针。

**memset**

- memset(&state, 0, sizeof(state)); 用于初始化 state 结构体，将其所有成员变量清零，确保初始状态一致，避免未初始化变量带来的不可预测行为。