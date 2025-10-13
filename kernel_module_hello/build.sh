#!/bin/bash
set -e

KDIR=/lib/modules/$(uname -r)/build   # 指向你的内核源码（或 /lib/modules/$(uname -r)/build）
# ====================================

PWD=$(pwd)
export ARCH CROSS_COMPILE

echo "[*] Using KDIR=${KDIR}"

make -C "${KDIR}" M=${PWD} modules
echo "[*] Build done: $(pwd)/hello.ko"
modinfo hello.ko | grep vermagic || true
