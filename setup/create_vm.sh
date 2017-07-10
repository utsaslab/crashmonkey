#! /usr/bin/sudo /bin/bash

CUR_DIR=`pwd`
DIR=$CUR_DIR/ubuntu-kvm
WORKSPACE=$CUR_DIR
NAME="$1"
IP="$2"

set -x
sudo vmbuilder kvm ubuntu \
  --libvirt qemu:///system \
  --overwrite \
  --arch amd64 \
  --mem 4096 \
  --rootsize 20480 \
  --user <USER_NAME> \
  --name <USER_NAME> \
  --flavour virtual \
  --suite trusty \
  --addpkg linux-image-generic \
  --addpkg openssh-server \
  --addpkg strace \
  --addpkg vim \
  --addpkg gcc \
  --addpkg tmux \
  --addpkg make \
  --addpkg git \
  --addpkg g++ \
  --network=em1 \
  --bridge=br0 \
  --hostname="$NAME" \
  --ip "$IP" \
  --gw 192.168.1.1 \
  --dns 8.8.8.8 \
  --copy "$WORKSPACE/rcs"

if [ $? -eq 0 ]; then
  mv $DIR/*.qcow2 "$WORKSPACE/$NAME.qcow2"
  chown -R `whoami`:`whoami` "$WORKSPACE"
fi
