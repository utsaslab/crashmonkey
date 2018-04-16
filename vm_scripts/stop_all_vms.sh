#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide number of vms as the parameter;"
    exit 1
fi

num_vms=$1

for i in `seq 1 $num_vms`; do
	VBoxManage controlvm ubuntu16-vm"$i" poweroff
done

