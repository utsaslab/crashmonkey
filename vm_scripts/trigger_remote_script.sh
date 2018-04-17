#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide number of vms and remote script to trigger as the parameters;"
    exit 1
fi

num_vms=$1
remote_script=$2

port=3022
for i in `seq 1 $num_vms`; do
	rsh -p $port pandian@127.0.0.1 "echo alohomora| sudo -S bash "$remote_script
	port=`expr $port + 1`
done
