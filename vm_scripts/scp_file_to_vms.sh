#!/bin/bash

# Given a file path, scp it to all the VMs running (to the home path)

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide number of vms and the path to the file as the parameters;"
    exit 1
fi

num_vms=$1
file_to_scp=$2

port=3022
for i in `seq 1 $num_vms`; do
	scp -P $port $file_to_scp pandian@127.0.0.1:~/
	port=`expr $port + 1`
done
