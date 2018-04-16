#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide number of vms as the parameter;"
    exit 1
fi

num_vms=$1

port=3022
for i in `seq 1 $num_vms`; do
	rsh -p $port pandian@127.0.0.1 "echo alohomora| sudo -S bash ~/vm_remote_script.sh"
	port=`expr $port + 1`
done
