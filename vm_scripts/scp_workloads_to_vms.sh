#!/bin/bash

num_vms=$num_vms

# Copy the other remote scripts
./scp_remote_scripts_to_vms.sh

port=3022
for i in `seq 1 $num_vms`; do
	echo `date` ' SCPing workloads to VM '$i'...'

	rsh -p $port pandian@127.0.0.1 "rm -r ~/seq2"
	rsh -p $port pandian@127.0.0.1 "mkdir -p ~/seq2"
	
	# Copy the workloads
	scp -P $port ~/seq2/vm$i/* pandian@127.0.0.1:~/seq2/
	
	port=`expr $port + 1`
done

echo `date` ' SCP workloads complete.'
