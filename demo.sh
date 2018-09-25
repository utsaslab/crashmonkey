#!/bin/bash

if [ "$#" -ne 2 ]; then
	echo "Usage : ./demo.sh <FS> <outfile>"
	exit 1
fi

FS=$1
outfile=$2
WORKLOAD_DIR="code/tests/seq1_demo"
TARGET_DIR="code/tests/generated_workloads"
TARGET_BUILD_DIR="build/tests/generated_workloads/"
REPORT_DIR="diff_results"

# Starting workload generation..
# Let's use the restricted bounds defined by -d flag in the workload generator
cd ace
echo "Cleaning up the target workload directory"
if [ -d "$WORKLOAD_DIR" ]; then rm -rf $WORKLOAD_DIR; fi

echo "Starting workload generation.."
start=`date +%s.%3N`
python ace.py -l 1 -n False -d True

end_gen=`date +%s.%3N`
# Now let's compile the generated tests
cd ..

# Before starting compilation, let's cleanup the target directories, just to be sure we'll run only the demo workloads. Also, copy generated workloads to TARGET_DIR
if [ -d "$TARGET_DIR" ]; then rm -rf $TARGET_DIR/*; fi
cp $WORKLOAD_DIR/j-lang*.cpp $TARGET_DIR/

echo "Workload generation complete. Compiling workloads.."
make gentests > out_compile 2>&1

end_compile=`date +%s.%3N`

# The workloads are now compiled and placed under TARGET_BUILD_DIR. 
# Run the workloads
echo -e "\nCompleted compilation. Testing workloads on $FS.."
if [ -d "$REPORT_DIR" ]; then rm -rf $REPORT_DIR; fi
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t $FS -e 102400 -u $TARGET_BUILD_DIR | tee $outfile

end=`date +%s.%3N`
run_time=$( echo "$end - $start" | bc -l)

echo -e "====================================\n"
printf "Demo Completed in %.2f seconds\n" $run_time
gen_time=$( echo "$end_gen - $start" | bc -l )
compile_time=$( echo "$end_compile - $end_gen" | bc -l )
test_time=$( echo "$end - $end_compile" | bc -l )

echo -e "------------------------"
printf "Generation   \t %.2f s" $gen_time
printf "\nCompilation\t %.2f s" $compile_time
printf "\nTesting    \t %.2f s" $test_time 
echo -e "\n------------------------\n"
echo -e "See complete bug reports at diff-results/\n"

