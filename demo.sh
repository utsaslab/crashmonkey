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
start=$SECONDS
python ace.py -l 1 -n False -d True

# Now let's compile the generated tests
cd ..

# Before starting compilation, let's cleanup the target directories, just to be sure we'll run only the demo workloads. Also, copy generated workloads to TARGET_DIR
if [ -d "$TARGET_DIR" ]; then rm -rf $TARGET_DIR/*; fi
cp $WORKLOAD_DIR/j-lang*.cpp $TARGET_DIR/

# Start compilation 
echo "Workload generation complete. Compiling workloads.."
make gentests > out_compile 2>&1

# The workloads are now compiled and placed under TARGET_BUILD_DIR. 
# Run the workloads
echo -e "\nCompleted compilation. Testing workloads on $FS.."
if [ -d "$REPORT_DIR" ]; then rm -rf $REPORT_DIR; fi
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t $FS -e 102400 -u $TARGET_BUILD_DIR | tee $outfile

end=$(( SECONDS - start ))

echo -e "\nDemo Completed in $end seconds \n See run log at $outfile and bug reports at diff-results/"

