#!/bin/bash

num_vms=$num_vms

for i in `seq 1 $num_vms`; do
	VBoxManage controlvm ubuntu16-vm"$i" poweroff
done

