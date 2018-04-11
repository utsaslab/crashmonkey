#!/bin/bash
_file="$1"
_target="$2"
[ $# -eq 0 ] && { echo "Usage: $0 filename"; exit 1; }
[ ! -f "$_file" ] && { echo "Error: $0 file not found."; exit 2; }
 
if [ -s "$_file" ] 
then
	echo "$_file : Failed test : Non empty diff"
	tail -vn +1 build/diff* >> diff_results/$_target
	rm build/diff*
else
    rm build/diff*
	echo "$_file : Passed test"
fi
