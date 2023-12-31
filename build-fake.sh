#!/bin/sh
#
# qemu fake build script to run on GITLAB/GITHUB for CI/CD pipeline testing.
# ====== (c) 11/03/2023 ======
#

if [ $# -ne 2 ];
then
  echo "Usage: build-fake.sh <PREFIX - path for installed QEMU files> <BUILD number> "
  exit 0;
fi

PREFIX=$1
BUILDN=$2

echo "Install prefix <$PREFIX> build=<$BUILDN>"

mkdir $PREFIX

QEMU_ROOT=`pwd`
echo "----   QEMU root = <${QEMU_ROOT}> ----"

PREFIX_FOR_CONFIGURE=${QEMU_ROOT}/${PREFIX}
echo "---- QEMU install preix = <${PREFIX_FOR_CONFIGURE}> ----"

cp qem* $PREFIX
cd $PREFIX
tar cvf ../Files-qemu.${BUILDN}.tar qem*
rm qem*
mv ../Files-qemu.${BUILDN}.tar .
cd -

echo "Building QEMU .... "
echo "Installing QEMU files ...."