#!/bin/bash

#scp all the remote vm scripts to all the VMs running

num_vms=$num_vms

port=3022
for i in `seq 1 $num_vms`; do
        scp -P $port ~/vm_remote_* pandian@127.0.0.1:~/
        scp -P $port ~/cm_cleanup.sh pandian@127.0.0.1:~/

	port=`expr $port + 1`
done
