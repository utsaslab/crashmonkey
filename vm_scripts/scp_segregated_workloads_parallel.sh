#!/bin/bash

st=1
end=5

for i in `seq 1 10`; do
	nohup ./scp_segregated_workloads.sh $st $end > out$i.log &
	st=`expr $st + 5`
	end=`expr $end + 5`
done
