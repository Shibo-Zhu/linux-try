#!/bin/bash
# 一键运行 hello 模块实验脚本
# 在 Ubuntu 上直接运行: bash run.sh

set -e  # 出错立即退出
LOGFILE="run.log"

echo "=======================================" | tee $LOGFILE
echo "[+] Hello Kernel Module Experiment" | tee -a $LOGFILE
echo "=======================================" | tee -a $LOGFILE

# 1️⃣ 清理旧文件
echo "[*] Cleaning old builds..." | tee -a $LOGFILE
make clean >> $LOGFILE 2>&1

# 2️⃣ 编译模块
echo "[*] Building module..." | tee -a $LOGFILE
make >> $LOGFILE 2>&1

if [ ! -f hello.ko ]; then
    echo "[x] Build failed! See $LOGFILE for details." | tee -a $LOGFILE
    exit 1
fi

# 3️⃣ 插入模块
echo "[*] Inserting module..." | tee -a $LOGFILE
sudo dmesg -C  # 清空旧日志
sudo insmod hello.ko >> $LOGFILE 2>&1

# 4️⃣ 查看 dmesg 输出
echo "[*] Kernel log after insert:" | tee -a $LOGFILE
dmesg | tail -n 5 | tee -a $LOGFILE

# 5️⃣ 列出已加载模块
echo "[*] Checking loaded modules:" | tee -a $LOGFILE
lsmod | grep hello | tee -a $LOGFILE || echo "(not found)" | tee -a $LOGFILE

# 6️⃣ 卸载模块
echo "[*] Removing module..." | tee -a $LOGFILE
sudo rmmod hello >> $LOGFILE 2>&1

# 7️⃣ 查看卸载日志
echo "[*] Kernel log after remove:" | tee -a $LOGFILE
dmesg | tail -n 5 | tee -a $LOGFILE

echo "[✓] Done. Full logs in $LOGFILE"
