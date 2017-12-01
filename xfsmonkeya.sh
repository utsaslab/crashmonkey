TEST_TYPE="ext4"
FIRST_TEST=1
LAST_TEST=5
SCRATCH_DEVICE="/dev/sda"
SCRATCH_MOUNT="/mnt/sdbmount"
OUTPUT_DIR="./output"

for((testNumber=$FIRST_TEST; testNumber<=$LAST_TEST; testNumber++))
do
	printf -v paddedNumber "%03d" $testNumber

	TEST_CASE_DIR=$OUTPUT_DIR/$TEST_TYPE/$paddedNumber

	sudo mkdir -p $TEST_CASE_DIR
	sudo touch $TEST_CASE_DIR/out.txt > /dev/null
	sudo chmod  777 $TEST_CASE_DIR/out.txt > /dev/null

	echo "About to run test case $TEST_TYPE/$paddedNumber"

	sudo ./xfsmonkey.py --primary-dev TEST -t $SCRATCH_DEVICE $SCRATCH_MOUNT -e $TEST_TYPE/$paddedNumber > $TEST_CASE_DIR/out.txt 2>&1
	sudo mv ./build/cm_out* $OUTPUT_DIR/$TEST_TYPE/$paddedNumber/

	echo "Ran test case $TEST_TYPE/$paddedNumber"
done


