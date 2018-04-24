#!/bin/bash

for i in `ps aux | grep startvm | grep -v grep | tr -s ' ' | cut -d ' ' -f2`; 
do 
	kill $i; 
done

