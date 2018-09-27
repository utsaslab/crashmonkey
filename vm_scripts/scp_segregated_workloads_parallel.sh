#!/bin/bash


if [ "$#" -ne 3 ]; then
    echo "Illegal number of parameters; Please provide batch size, num_servers, and segregated workload path as the parameters;"
    exit 1
fi

batch_size=$1
st=1
end=`expr $st + $batch_size - 1`
num_servers=$2
seg_path=$3

i=1
while [ $st -le $num_servers ]; do
	nohup ./scp_segregated_workloads.sh $st $end $seg_path > out"$i".log &
	st=`expr $st + $batch_size`
	end=`expr $end + $batch_size`
	i=`expr $i + 1`
done
