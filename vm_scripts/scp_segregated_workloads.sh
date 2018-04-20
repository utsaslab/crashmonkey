#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide start server number and end server number as the parameters;"
    exit 1
fi

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

	scp -r -o "StrictHostKeyChecking no" -i ~/crashmonkey.pem workloads/seg/node"$i"-"$ip"/* cc@$ip:~/seq2/

	i=`expr $i + 1`
done
