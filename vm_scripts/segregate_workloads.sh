#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters; Please provide num workloads per vm, max workload number and base path to workload files as the parameters;"
    exit 1
fi

i=1
k=1
num_per_vm=$1
max=$2
workload_base_path=$3
num_vms=12

for ip in `cat live_nodes`; do
	echo `date` ------------- Segregating to node $i IP $ip starting from k $k -----------------
	mkdir -p workloads/seg/node"$i"-"$ip"

	for j in `seq 1 $num_vms`; do
		echo `date` Segregating to vm $j of node $i ...
        	mkdir -p workloads/seg/node"$i"-"$ip"/vm"$j"
		
		for l in `seq 1 $num_per_vm`; do
			cp $workload_base_path/j-lang"$k".cpp workloads/seg/node"$i"-"$ip"/vm"$j"/
			k=`expr $k + 1`
			
			if [ $k -gt $max ]
			then
				break
			fi
		done
		if [ $k -gt $max ]
                then
                	break
                fi
	done
	if [ $k -gt $max ]
        then
        	break
        fi

	i=`expr $i + 1`
done
