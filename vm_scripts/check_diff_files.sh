#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide start server number and end server number as the parameters;"
    exit 1
fi

i=1
st=$1
end=$2

mkdir -p diff_files
for ip in `cat live_nodes`; do
        echo `date` ------------- Checking for diff file from node $i IP $ip -----------------

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

	port=3022
	for j in `seq 1 $num_vms`; do
		sshpass -p "alohomora" scp -o "StrictHostKeyChecking no" -P $port pandian@$ip:~/projects/crashmonkey/diff_results/* diff_files/
		port=`expr $port + 1`
	done
	i=`expr $i + 1`
done
