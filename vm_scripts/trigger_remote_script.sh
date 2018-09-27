#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide remote script to trigger as the parameter;"
    exit 1
fi

num_vms=$num_vms
remote_script=$1

port=3022
for i in `seq 1 $num_vms`; do
	echo ----- Triggering on VM $i -----
	rsh -p $port user@127.0.0.1 "echo password| sudo -S bash "$remote_script
	port=`expr $port + 1`
done
