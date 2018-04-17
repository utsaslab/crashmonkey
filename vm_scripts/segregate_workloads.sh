#!/bin/bash

i=1
k=1
num_per_vm=87
max=65000

for ip in `cat live_nodes`; do
	echo `date` ------------- Segregating to node $i IP $ip starting from k $k -----------------
	mkdir -p workloads/seg/node"$i"-"$ip"

	if [ $i -le 6 ]
	then
		num_vms=8
	else
		num_vms=16
	fi

	for j in `seq 1 $num_vms`; do
		echo `date` Segregating to vm $j of node $i ...
        	mkdir -p workloads/seg/node"$i"-"$ip"/vm"$j"
		
		for l in `seq 1 $num_per_vm`; do
			cp workloads/seq2/j-lang"$k".cpp workloads/seg/node"$i"-"$ip"/vm"$j"/
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
