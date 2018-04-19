#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide file system as the parameter;"
    exit 1
fi

num_vms=$num_vms
fs=$1

port=3022
for i in `seq 1 $num_vms`; do
	rsh -p $port pandian@127.0.0.1 "echo alohomora| sudo -S bash /home/pandian/vm_remote_trigger_script.sh "$fs
	port=`expr $port + 1`
done
