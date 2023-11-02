#
# 10/26/2023 - The very first DOCKER for QEMU build-and-run environment
#
FROM ubuntu:20.04
#
# QEMU root plus the directories for the files to run linux kernel 
#
RUN  mkdir -p /qemu-root
RUN  mkdir -p /qemu-root/linux-setup
RUN  mkdir -p /qemu-root/scripts
RUN  mkdir -p /qemu-root/install-qemu-rv
COPY qemu-deps.sh /qemu-root/scripts

#
# Linux kernel to boot linux on QEMU
# Linux ROOT file system
#
#  COPY Image /qemu-root/linux-setup
#  COPY rootfs.ext2 /qemu-root/linux-setup
#
# Install necessary packages
#
RUN apt update
RUN apt-get -y install make

#
# The below ARG command helps to avoid an interactive mode
# when installing pkg-config
#
ARG DEBIAN_FRONTEND=noninteractive
RUN apt -y install pkg-config
RUN apt-get -y install gcc
RUN apt-get -y install git

RUN apt-get -y install autoconf
RUN apt-get -y install automake
RUN apt-get -y install autotools-dev
RUN apt-get -y install curl
RUN apt-get -y install python3
RUN apt-get -y install python3-pip
RUN apt-get -y install libmpc-dev
RUN apt-get -y install libmpfr-dev
RUN apt-get -y install libgmp-dev
RUN apt-get -y install gawk
RUN apt-get -y install build-essential
RUN apt-get -y install bison
RUN apt-get -y install ninja-build
RUN apt-get -y install python3-venv

RUN apt -y install libslirp-dev 
RUN apt -y install libglib2.0-dev
RUN apt-get -y install libpixman-1-dev




