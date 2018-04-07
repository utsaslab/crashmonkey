#!/bin/bash
_file="$1"
_target="$2"
[ $# -eq 0 ] && { echo "Usage: $0 filename"; exit 1; }
[ ! -f "$_file" ] && { echo "Error: $0 file not found."; exit 2; }
 
if [ -s "$_file" ] 
then
	echo "$_file : Non empty diff"
	tail -vn +1 build/diff* >> diff_results/$_target
	rm build/diff*
        # do something as file has data
else
	echo "$_file : Passed test"
        # do something as file is empty 
fi
