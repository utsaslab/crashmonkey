#!/bin/bash

num_vms=$num_vms
remote_script='vm_remote_update_hostname_script.sh'

port=3022
for i in `seq 1 $num_vms`; do
	sshpass -p "alohomora" rsh -o "StrictHostKeyChecking no" -p $port pandian@127.0.0.1 "echo alohomora| sudo -S bash "$remote_script" "$i
	port=`expr $port + 1`
done
