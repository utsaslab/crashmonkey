TEST_TYPE="generic"
FIRST_TEST=11
LAST_TEST=100
SCRATCH_DEVICE="/dev/sdb"
SCRATCH_MOUNT="/mnt/sdbmount"
OUTPUT_DIR="./output"
ITERATIONS=100

SUMMARY_FILE='./summary'

rm -f $SUMMARY_FILE

for((testNumber=$FIRST_TEST; testNumber<=$LAST_TEST; testNumber++))
do
	printf -v paddedNumber "%03d" $testNumber

	TEST_CASE_DIR=$OUTPUT_DIR/$TEST_TYPE/$paddedNumber

	sudo mkdir -p $TEST_CASE_DIR
	sudo touch $TEST_CASE_DIR/out.txt > /dev/null
	sudo chmod  777 $TEST_CASE_DIR/out.txt > /dev/null

	printf "About to run test case \033[0;34m$TEST_TYPE/$paddedNumber\033[m\n"

	sudo ./xfsmonkey.py --primary-dev TEST -t $SCRATCH_DEVICE $SCRATCH_MOUNT -i $ITERATIONS -e $TEST_TYPE/$paddedNumber > $TEST_CASE_DIR/out.txt 2>&1
	sudo mv ./build/cm_out* $TEST_CASE_DIR

	printf "Ran test case \033[0;34m$TEST_TYPE/$paddedNumber\033[m\n"

  fixed=$(grep "passed fixed: 0" $TEST_CASE_DIR/out.txt)
	failed=$(grep "failed: 0" $TEST_CASE_DIR/out.txt)
	passed=$(grep "passed cleanly:" $TEST_CASE_DIR/out.txt)
	if [[ -z $fixed ]] || [[ -z $failed ]]
	then
	  if [[ -z $fixed ]]
		then
			printf "\033[0;31mPASSED FIXED: $TEST_CASE_DIR/out.txt\033[m\n" | tee -a $SUMMARY_FILE
		fi
		if [[ -z $failed ]]
		then
			printf "\033[0;31mFAILED: $TEST_CASE_DIR/out.txt\033[m\n" | tee -a $SUMMARY_FILE
		fi
		printf "$passed\n"
	else
		printf "\033[0;32mNo problems\033[m\n"
		sudo rm -f $TEST_CASE_DIR/cm_out*
		printf "Deleted snap and profile file for this run\n"
	fi
	printf "###############################################\n"
done
