#!/bin/bash

num=`ps aux | grep xfsMonkey | grep -v grep | wc -l`

if [ $num -eq 0 ]
then
	echo `uname -n` 'No runs in progress'
else
	echo `uname -n` 'Some run is in progress'
fi
