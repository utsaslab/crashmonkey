#!/bin/bash

num_vms=$num_vms
port=3022
i=1

while [ $i -le $num_vms ]; do
	res_raw=`timeout -s KILL 10 rsh -p $port user@127.0.0.1 "touch /home/user/a 2>&1; echo abc;"`
	res=`echo $res_raw | grep 'Read-only'`

	if [ -z $res_raw ] || [ ! -z "$res" ];
	then
                echo `date` 'VM '$i' is read-only. Restarting it.. '
                ./force_stop_vm.sh $i
		sleep 3
                ./start_particular_vm.sh $i
                echo `date` 'Sleeping for 10 seconds..'
                sleep 10
	else
		echo `date` 'VM '$i' is fine.'
	fi
	i=`expr $i + 1`
	port=`expr $port + 1`
done
