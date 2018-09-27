#!/bin/bash

num_vms=$num_vms
port=3022
i=1

while [ $i -le $num_vms ]; do
	timeout -s KILL 10 sshpass -p "password" scp -o "StrictHostKeyChecking no" -P $port ~/vm_remote_* user@127.0.0.1:~/
	ex=$?

	if [ $ex -ne 0 ];
	then
                echo `date` 'SCP to VM '$i' was not successful. Restarting it.. '
                ./force_stop_vm.sh $i
		sleep 3
                ./start_particular_vm.sh $i
                echo `date` 'Sleeping for 10 seconds..'
                sleep 10
	else
		echo `date` 'SCP to VM '$i' is working fine.'
	fi
	i=`expr $i + 1`
	port=`expr $port + 1`
done
