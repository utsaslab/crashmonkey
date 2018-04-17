#!/bin/bash

file=$1
hosts=`cat $file | tr '\r\n' ' '`
cssh -l cc $hosts
	
