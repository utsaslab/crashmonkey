#!/bin/bash

for i in `ps aux | grep -e xfsMonkey -e c_harness | grep -v grep | tr -s ' ' | cut -d ' ' -f2`; do
	echo password | sudo -S kill -9 $i
done
