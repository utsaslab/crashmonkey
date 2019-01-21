#!/bin/bash
_file="$1"
_target="$2"
_demo=${3:-0}

#Let's set color codes for passed and failed tests
red=`tput setaf 1`
green=`tput setaf 2`
bgcolor=`tput setab 7`
yellow=`tput setaf 3`
reset=`tput sgr0`
bold=`tput bold`

[ $# -eq 0 ] && { echo "Usage: $0 filename"; exit 1; }
[ ! -f "$_file" ] && { echo "Error: $0 file not found."; exit 2; }
 
if [ -s "$_file" ] 
then
	echo -e "${red}${bold} : Failed test${reset}"
	cat $_file >> diff_results/$_target
	if [ $_demo -eq 1 ]
	then
		source find_diff.sh $_file
	fi
	rm build/diff*
else
	if [ -n "$(ls build | grep diff)" ]
	then
   		rm build/diff*
		echo -e "${green}${bold} : Passed test${reset}"
	else	
		echo -e "${yellow}${bold} : Could not run test${reset}"
	fi
fi
