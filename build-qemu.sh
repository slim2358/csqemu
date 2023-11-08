
#!/bin/sh
#
# QEMU build script to run on GITLAB/GITHUB for CI/CD pipeline to build 
# QEMU binaries, libraries and plugings 
# ====== (c) 11/06/2023 ======
#

if [ $# -ne 2 ];
then
  echo "Usage: build-qemu.sh <PREFIX - path for installed QEMU files> <BUILD number> "
  exit 0;
fi

# 
# For now the ony BUILD NUMBER is passed. Essentially it is just an arbitrary number.
# In GITLAB context the NUMBER is one of predefined variables (CI_JOB_ID for example).
# In future the version number will be also passed.
#
# Please note! For GITLAB/DOCKER environemnt PREFIX is supposed to be "relative off qemu root".
# Please note! This script likely could be used in GITHUB as well.
#
# TBD: The build qemu.so will be added
#

PREFIX=$1
BUILDN=$2

echo "Install PERFIX = <$PREFIX>"
echo "BUILD number   = <$BUILDN>"

mkdir $PREFIX

QEMU_ROOT=`pwd`
echo "----   QEMU root = <${QEMU_ROOT}> ----"

PREFIX_FOR_CONFIGURE=${QEMU_ROOT}/${PREFIX}
echo "---- QEMU install preix = <${PREFIX_FOR_CONFIGURE}> ----"

echo "===================== Configure ..... ========================"
./configure --prefix="${PREFIX_FOR_CONFIGURE}" --target-list="riscv64-linux-user,riscv64-softmmu"

if [ $? -ne 0 ];
then
  echo "Failed: Configure RC = $?"
  exit 0;
fi

echo "===================== Make ..... ========================"
make

if [ $? -ne 0 ];
then
  echo "Failed: Make RC = $?"
  exit 0;
fi

echo "===================== Make install ..... ========================"
make install

echo "===================== Make tarball  ..... ========================"
tar -C INSTALL -cvf QEMU.bld.${BUILDN}.tar .
mv QEMU.bld.${BUILDN}.tar INSTALL
