#!/bin/bash
#
# The script to run baremetal code sample
# ====== (c) 11/14/2023 ======
#
#
#  Run in QEMU ROOT directory !!!!!!
#  TBD - more flexibility
# 

if [ $# -ne 2 ] && [ $# -ne 3 ];
then
  echo "Usage: scripts/run-baremetal.sh <PREFIX - path for installed QEMU files> <binary file> [<pligin>] "
  echo "Example: scripts/run-baremetal.sh  install-local  hello [sandbox] "
  exit 0
fi

PREFIX=$1
HELLO=$2

if [ $# -eq 3 ];
then
  PLUGIN=$3  
fi

QEMU_ROOT=`pwd`

#
# To distinguish between an absolute and relative path.
#
if [[ $PREFIX == /* ]];
then

  PREFIX_FOR_INSTALL=${PREFIX}

else

  PREFIX_FOR_INSTALL=${QEMU_ROOT}/${PREFIX}

fi

# One instruction per TB 
# No chaining (not used so far)

if [ $# -eq 3 ];
then
  ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d prefix:mytest -plugin ${PREFIX_FOR_INSTALL}/plugins/lib${PLUGIN}.so   -machine virt -bios ${HELLO}
else
  ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d nochain,prefix:mylog   -machine virt -bios ${HELLO}
fi

###  ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d op,in_asm,prefix:mytest -plugin ${PREFIX_FOR_INSTALL}/plugins/lib${PLUGIN}.so   -machine virt -bios ${HELLO}
###  ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d in_asm -d nochain -d prefix:mytest -machine virt -bios ${HELLO}
###  ${PREFIX_FOR_INSTALL}/bin/qemu-system-riscv64 -nographic -accel tcg,one-insn-per-tb=on -d nochain,prefix:mylog   -machine virt -bios ${HELLO}