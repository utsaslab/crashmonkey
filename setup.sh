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

function is_module_loaded() {
  lsmod | grep "$MODULE_NAME" &> /dev/null
  return $?
}

function load_module() {
  sudo insmod "$MODULE_PATH"
  return $?
}

function unload_module() {
  sudo rmmod "$MODULE_NAME"
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

function pv_create() {
  sudo pvcreate "$PV_DISK"
  return $?
}

function has_pv_disk() {
  sudo pvdisplay | grep "$PV_DISK"
  return $?
}

function pv_remove() {
  sudo pvremove -f "$PV_DISK"
  return $?
}

function vg_create() {
  sudo vgcreate "$VG" "$PV_DISK"
  return $?
}

function has_vg() {
  sudo vgdisplay | grep "$VG"
  return $?
}

function vg_remove() {
  sudo vgremove -f "$VG"
  return $?
}

function lv_create() {
  sudo lvcreate "$VG" -n "$LV" -l 50%FREE
  return $?
}

function has_lv() {
  sudo lvdisplay | grep "$LV"
  return $?
}

function lv_remove() {
  sudo lvremove -f "/dev/$VG/$LV"
  return $?
}

function snapshot_create() {
  sudo lvcreate -s -n "$SNAPSHOT_NAME" -l 100%FREE "/dev/$VG/$LV"
  return $?
}

function snapshot_remove() {
  sudo lvremove -f "/dev/$VG/$SNAPSHOT_NAME"
  return $?
}

function format_drive() {
  sudo mkfs -t ext4 "$DEVICE_PATH"
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
echo "$TEST_DIRTY_TIME" | sudo tee "/proc/sys/vm/dirty_expire_centisecs"
pv_create
if [[ "$?" -ne 0 ]] ; then
  echo "Error making physical volume"
  teardown 7
  exit 1
fi
vg_create
if [[ "$?" -ne 0 ]] ; then
  echo "Error making volume group"
  teardown 6
  exit 1
fi
lv_create
if [[ "$?" -ne 0 ]] ; then
  echo "Error making logical volume"
  teardown 5
  exit 1
fi
snapshot_create
if [[ "$?" -ne 0 ]] ; then
  echo "Error making snapshot of volume"
  teardown 4
  exit 1
fi
is_module_loaded
if [[ "$?" -eq 1 ]] ; then
  load_module
  if [[ "$?" -ne 0 ]] ; then
    echo "Error inserting module into kernel"
    teardown 3
    exit 1
  fi
fi
format_drive
if [[ "$?" -ne 0 ]] ; then
  echo "Error formatting test drive"
  teardown 1
  exit 1
fi
is_wrapper_mounted
if [[ "$?" -eq 1 ]] ; then
  mount_wrapper
  if [[ "$?" -ne 0 ]] ; then
    echo "Error mounting wrapper device"
    teardown 2
    exit 1
  fi
fi
exit
