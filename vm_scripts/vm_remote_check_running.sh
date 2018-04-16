#!/bin/bash

num=`ps aux | grep xfs | grep -v grep | wc -l`

if [ $num -eq 0 ]
then
	echo 'No runs in progress'
else
	echo 'Some run is in progress'
fi
