#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide the file with list of IPs the parameters;"
    exit 1
fi

file=$1
for i in `cat $1`; do 
	echo -------------- IP $i -------------; 
	ping -c1 -W1 $i && echo 'server '$i' is up' || echo 'server '$i' is down'; 
done
