#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide file system as the parameter;"
    exit 1
fi

num_vms=$num_vms
fs=$1

port=3022
for i in `seq 1 $num_vms`; do
	echo `date` 'Triggering xfsMonkey in VM '$i'...'
	rsh -p $port pandian@127.0.0.1 "nohup echo alohomora| sudo -S bash /home/pandian/vm_remote_trigger_script.sh "$fs" &> trigger_"$fs".log &"
	echo Sleeping for 10 seconds..
	sleep 10

	echo `date` ' Checking if trigger process is running in VM '$i'...'
	rsh -p $port pandian@127.0.0.1 "ps aux | grep -e xfsMonkey -e vm_remote_trigger"
	echo Sleeping for 5 seconds..
	sleep 5

	port=`expr $port + 1`
done
