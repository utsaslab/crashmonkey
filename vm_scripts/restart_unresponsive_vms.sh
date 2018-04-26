#!/bin/bash

num_vms=$num_vms
port=3022
i=1

while [ $i -le $num_vms ]; do
	res=`rsh -p $port pandian@127.0.0.1 "touch /home/pandian/a; echo abc"`
	
	if [ -z "$res" ];
	then
                echo 'VM '$i' is unresponsive. Restarting it.. '
                ./force_stop_vm.sh $i
		sleep 3
                ./start_particular_vm.sh $i
                echo 'Sleeping for 10 seconds..'
                sleep 10
	else
		echo 'VM '$i' is fine.'
	fi
	i=`expr $i + 1`
	port=`expr $port + 1`
done
