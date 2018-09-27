#!/bin/bash

if [ "$#" -ne 5 ]; then
    echo "Illegal number of parameters; Please provide num workloads per vm, max workload number, base path to workload files, batch size and num_servers as the parameter;"
    exit 1
fi

num_per_vm=$1
max_workload=$2
workload_base_path=$3
batch_size=$4
num_servers=$5

st=1
end=`expr $st + $batch_size - 1`

rm -r workloads/seg

i=1
while [ $st -le $num_servers ]; do
	echo Triggering ./segregate_workloads_range.sh $num_per_vm $max_workload $workload_base_path $st $end
        nohup ./segregate_workloads_range.sh $num_per_vm $max_workload $workload_base_path $st $end > out$i.log &
	st=`expr $st + $batch_size`
        end=`expr $end + $batch_size`
        i=`expr $i + 1`
done
