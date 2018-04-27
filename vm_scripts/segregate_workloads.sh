#!/bin/bash

if [ "$#" -ne 4 ]; then
    echo "Illegal number of parameters; Please provide num workloads per vm, start workload number, max workload number, base path to workload files, and output seg directory path (workloads/seq3-nested-seg1 for example) as the parameters;"
    exit 1
fi

i=1

num_per_vm=$1
k=$2
max=$3
workload_base_path=$4
output_workload_path=$5

num_vms=12

rm -r $output_workload_path

for ip in `cat live_nodes`; do
	echo `date` ------------- Segregating to node $i IP $ip starting from k $k -----------------
	mkdir -p $output_workload_path/node"$i"-"$ip"

	for j in `seq 1 $num_vms`; do
		echo `date` Segregating to vm $j of node $i ...
        	mkdir -p $output_workload_path/node"$i"-"$ip"/vm"$j"
		
		for l in `seq 1 $num_per_vm`; do
			cp $workload_base_path/j-lang"$k".cpp $output_workload_path/node"$i"-"$ip"/vm"$j"/
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
