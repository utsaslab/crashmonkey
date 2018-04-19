#!/bin/bash

# Given a file path, scp it to all the VMs running (to the home path)

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide the path to the file as the parameter;"
    exit 1
fi

num_vms=$num_vms
file_to_scp=$1

port=3022
for i in `seq 1 $num_vms`; do
	scp -P $port $file_to_scp pandian@127.0.0.1:~/
	port=`expr $port + 1`
done
