#!/bin/bash

umount /mnt/snapshot
rmmod disk_wrapper.ko
rmmod cow_brd.ko

