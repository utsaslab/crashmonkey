#!/bin/bash
_file="$1"

#Let's set color codes for passed and failed tests
red=`tput setaf 1`
green=`tput setaf 2`
bgcolor=`tput setab 7`
reset=`tput sgr0`
bold=`tput bold`

stat_diff="DIFF: Content Mismatch"
file_missing="Failed stating"
print_all=0
stat=0
index=0
declare -a Inode
declare -a Size
declare -a BSize
declare -a NumBlocks
declare -a Links

tput setaf 1

num_bugs="bugs"
num_missing="missing"
num_stat="stat"

#To output bug summary, let's just echo bug counts into appropriate files
bugs=`cat $num_bugs`
missing=`cat $num_missing`
mismatch=`cat $num_stat`

#If we reached this point, it means we encountered a bug. So increment bug count
bugs=$(( $bugs + 1))
echo $bugs > $num_bugs


#Let's read the diff file line by line and see where it differs
[ $# -eq 0 ] && { echo "Usage: $0 filename"; exit 1; }
[ ! -f "$_file" ] && { echo "Error: $0 file not found."; exit 2; }
 
if [ -s "$_file" ] 
then
	while read line; do
		if echo $line | grep -q "$stat_diff"
		then 
			file_name=$line 
			echo $line
			stat=1
		elif echo $line | grep -q "$file_missing"
		then
			echo $line
			print_all=1

		elif [ $stat -eq 1 ]
		then 
			if echo $line | grep -q "Inode"
			then
				Inode[$index]=`echo $line|cut -d ' ' -f3`
			elif echo $line | grep -q "TotalSize"
                        then
                                Size[$index]=`echo $line|cut -d ' ' -f3`
			elif echo $line | grep -q "BlockSize"
                        then
                                BSize[$index]=`echo $line|cut -d ' ' -f3`
			elif echo $line | grep -q "#Blocks"
                        then
                                NumBlocks[$index]=`echo $line|cut -d ' ' -f3`
			elif echo $line | grep -q "HardLinks"
                        then
                                Links[$index]=`echo $line|cut -d ' ' -f2`
				index=$(( $index + 1 ))
			else
				:
			fi
			
		elif [ $print_all -eq 1 ]
		then
			echo $line

		else
			:
		fi
	done < $_file

	#Now that we've parsed the diff files, let's find out what is different from actual and expected states and print it.
	
	if [ $stat -eq 1 ]
	then
	
		mismatch=$(( $mismatch + 1))
		echo $mismatch > $num_stat

		if [[ "${Inode[0]}" -ne "${Inode[1]}" ]]
		then
			echo -e "\tExpected Inode Number = ${Inode[1]}\n\tActual Inode Number = ${Inode[0]}"
		 	
		elif [[ "${Size[0]}" -ne "${Size[1]}" ]]
		then
			echo -e "\tExpected File Size = ${Size[1]}\n\tActual File Size = ${Size[0]}"
		
		elif [[ ${BSize[0]} -ne ${BSize[1]} ]]
		then
			echo -e "\tExpected Block Size = ${BSize[1]}\n\tActual Block Size = ${BSize[0]}"
		
		elif [[ ${NumBlocks[0]} -ne ${NumBlocks[1]} ]]
		then
			echo -e "\tExpected Block Count = ${NumBlocks[1]}\n\tActual Block Count = ${NumBlocks[0]}"
		
		elif [[ ${Links[0]} -ne ${Links[1]} ]]
		then
			echo -e "\tExpected Link Count = ${Links[1]}\n\tActual Link Count = ${Links[0]}"
		
		else			
			:
		fi
	else	
		missing=$(( $missing + 1))
		echo $missing > $num_missing

	fi
fi

tput sgr0
