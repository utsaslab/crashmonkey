#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide number of vms as the parameter;"
    exit 1
fi

num_vms=$1

port=3022
for i in `seq 1 $num_vms`; do
	rsh -p $port pandian@127.0.0.1 "rm -r ~/seq2"
	rsh -p $port pandian@127.0.0.1 "mkdir -p ~/seq2"
	scp -P $port ~/seq2/vm$i/* pandian@127.0.0.1:~/seq2/
	scp -P $port ~/vm_remote_script.sh pandian@127.0.0.1:~/
	scp -P $port ~/cm_cleanup.sh pandian@127.0.0.1:~/
	port=`expr $port + 1`
done
