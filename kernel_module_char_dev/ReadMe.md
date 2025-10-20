## 功能目标
我们将创建一个名为 char_dev 的内核模块，它会：

- 在加载时，动态注册一个字符设备，并获得一个主设备号。

- 创建一个对应的设备文件 /dev/char_dev。

- 实现 read 和 write 操作：

- 用户向 /dev/char_dev 写入数据时，数据被存入内核模块中的一个缓冲区。

- 用户从 /dev/char_dev 读取数据时，模块将缓冲区中的数据返回给用户。

- 在卸载时，清理并注销设备和相关资源。

## 与设备交互 (写入和读取)
向设备写入数据
使用 echo 命令。sudo 是必需的，因为默认设备文件权限只允许 root 用户访问。

```bash
echo "Hello from user space!" | sudo tee /dev/char_dev
```

查看内核日志，你会看到模块打印的接收消息：

```bash
dmesg | tail -n 1
# [timestamp] CharDevModule: Received 23 characters from user: 'Hello from user space!'
```

从设备读取数据
使用 cat 命令。

```bash
sudo cat /dev/char_dev
# Hello from user space!
```
再次查看内核日志，你会看到模块的发送消息：

```bash
dmesg | tail -n 1
# [timestamp] CharDevModule: Sent 22 characters to the user
```
思考一下：为什么写入了23个字符，但读取时发送了22个？（提示：echo 命令默认会添加一个换行符 \n，我们在 dev_write 函数中处理了它。）