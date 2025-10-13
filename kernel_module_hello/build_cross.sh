#!/bin/bash
set -e

# ====== 请按实际情况修改这两项 ======
CROSS_COMPILE=aarch64-linux-gnu-   # 交叉前缀（替换为你的）
ARCH=arm64                         # 目标架构（arm/arm64/x86 等）
KDIR=/media/zs/ubuntu_disk/rt_linux/rpi-5.8/linux   # 指向你的内核源码（或 /lib/modules/$(uname -r)/build）
# ====================================

PWD=$(pwd)
export ARCH CROSS_COMPILE

echo "[*] Using KDIR=${KDIR}"
echo "[*] Building module for ARCH=${ARCH} with CROSS_COMPILE=${CROSS_COMPILE}"

make -C "${KDIR}" M=${PWD} ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
echo "[*] Build done: $(pwd)/hello.ko"
modinfo hello.ko | grep vermagic || true
