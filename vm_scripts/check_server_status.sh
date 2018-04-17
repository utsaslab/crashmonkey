#!/bin/bash

file=$1
for i in `cat $1`; do 
	echo -------------- IP $i -------------; 
	ping -c1 -W1 $i && echo 'server '$i' is up' || echo 'server '$i' is down'; 
done
