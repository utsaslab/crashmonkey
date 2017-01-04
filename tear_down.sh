#!/bin/bash

MODULE_NAME="hellow"
MODULE_PATH="/home/ashmrtn/research/hello/hellow.ko"
DEVICE_PATH="/dev/hwm"
MOUNT_PATH="/mnt/snapshot"
C_HARNESS="/home/ashmrtn/research/hello/c_harness"
TEST_DIRTY_TIME=1500
PV_DISK="/dev/ram0"
VG="fs_consist_test"
LV="fs_consist_test_dev"
SNAPSHOT_NAME="fs_consist_test_snap"

function unload_module() {
  sudo rmmod "$MODULE_NAME"
  return $?
}
function unmount_wrapper() {
  sudo umount "$MOUNT_PATH"
  return $?
}

function pv_remove() {
  sudo pvremove -f "$PV_DISK"
  return $?
}

function vg_remove() {
  sudo vgremove -f "$VG"
  return $?
}

function lv_remove() {
  sudo lvremove -f "/dev/$VG/$LV"
  return $?
}

function snapshot_remove() {
  sudo lvremove -f "/dev/$VG/$SNAPSHOT_NAME"
  return $?
}

function teardown() {
  case "$1" in
    1)
      unmount_wrapper
      ;&
    2)
      unload_module
      ;&
    3)
      snapshot_remove
      ;&
    4)
      lv_remove
      ;&
    5)
      vg_remove
      ;&
    6)
      pv_remove
      ;&
    7)
      echo "$OLD_DIRTY_TIME" | sudo tee "/proc/sys/vm/dirty_expire_centisecs"
      ;;
  esac
}

set -x
OLD_DIRTY_TIME="30000"
teardown $1
exit
