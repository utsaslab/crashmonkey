#!/bin/bash


if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters; Please provide file system and batch size the parameters;"
    exit 1
fi

num_vms=$num_vms

fs=$1
batch_size=$2
st=1
end=`expr $st + $batch_size - 1`

i=1
while [ $st -le $num_servers ]; do
	nohup ./trigger_remote_script_trigger_xfsMonkey_range.sh $fs $st $end > out_trigger_log_"$fs"_"$st"_"$end".log &
	st=`expr $st + $batch_size`
	end=`expr $end + $batch_size`
	i=`expr $i + 1`
done
