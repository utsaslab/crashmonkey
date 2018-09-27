#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters; Please provide start server number, end server number, and segregated workload path as the parameters;"
    exit 1
fi

i=1

st=$1
end=$2
seg_path=$3

for ip in `cat live_nodes`; do
	echo `date` ------------- SCPing segregated workloads to node $i IP $ip -----------------

	if [ $i -lt $st ] || [ $i -gt $end ]
	then
		i=`expr $i + 1`
		continue
	fi

	scp -r -o "StrictHostKeyChecking no" -i ~/crashmonkey.pem "$seg_path"/node"$i"-"$ip"/* cc@$ip:~/seq2/

	i=`expr $i + 1`
done
