#!/bin/bash

num_vms=$num_vms

for i in `seq 1 $num_vms`; do
	VBoxManage startvm ubuntu16-vm"$i" --type headless
	echo Sleeping for 15 seconds..
	sleep 15
done

