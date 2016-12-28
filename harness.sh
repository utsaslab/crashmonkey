#!/bin/bash

MODULE_NAME="hellow"
MODULE_PATH="/home/ashmrtn/research/hello/hellow.ko"
DEVICE_PATH="/dev/hwm1"
MOUNT_PATH="/mnt/snapshot"
C_HARNESS="/home/ashmrtn/research/hello/c_harness"
TEST_DIRTY_TIME=1500

function is_module_loaded() {
  lsmod | grep "$MODULE_NAME" &> /dev/null
  return $?
}

function load_module() {
  sudo insmod "$MODULE_PATH"
  return $?
}

function is_wrapper_mounted() {
  cat "/etc/mtab" | grep "$DEVICE_PATH" &> /dev/null
  return $?
}

function mount_wrapper() {
  sudo mount "$DEVICE_PATH" "$MOUNT_PATH"
  return $?
}

function unmount_wrapper() {
  sudo umount "$MOUNT_PATH"
  return $?
}

set -x
OLD_DIRTY_TIME="$(cat /proc/sys/vm/dirty_expire_centisecs)"
echo "$TEST_DIRTY_TIME" | sudo tee "/proc/sys/vm/dirty_expire_centisecs"
is_module_loaded
if [[ "$?" -eq 1 ]] ; then
  load_module
  if [[ "$?" -ne 0 ]] ; then
    echo "Error inserting module into kernel"
    exit 1
  fi
fi
is_wrapper_mounted
if [[ "$?" -eq 1 ]] ; then
  mount_wrapper
  if [[ "$?" -ne 0 ]] ; then
    echo "Error mounting wrapper device"
    exit 1
  fi
fi
sudo "$C_HARNESS"
echo "$OLD_DIRTY_TIME" | sudo tee "/proc/sys/vm/dirty_expire_centisecs"
exit
