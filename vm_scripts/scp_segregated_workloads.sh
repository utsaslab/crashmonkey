#!/bin/bash

i=1

st=$1
end=$2

for ip in `cat live_nodes`; do
	echo `date` ------------- SCPing segregated workloads to node $i IP $ip -----------------

	if [ $i -le 6 ]
	then
		num_vms=8
	else
		num_vms=16
	fi
	
	if [ $i -lt $st ] || [ $i -gt $end ]
	then
		i=`expr $i + 1`
		continue
	fi

	for j in `seq 1 $num_vms`; do
		scp -r -o "StrictHostKeyChecking no" -i ~/crashmonkey.pem workloads/seg/node"$i"-"$ip"/* cc@$ip:~/seq2/
	done

	i=`expr $i + 1`
done
