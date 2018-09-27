#!/bin/bash

completed=`cat /home/user/projects/crashmonkey/xfsmonkey*.log | grep Test | tail -n1 | cut -d# -f2 | cut -d ' ' -f2`
ram_disk_error=`cat /home/user/projects/crashmonkey/xfsmonkey*.log | grep 'Error inserting RAM disk module' | wc -l`
total=`ls -lah /home/user/projects/crashmonkey/build/xfsMonkeyTests/j-lang* | grep -v grep | wc -l`
num_diffs=`ls -lah /home/user/projects/crashmonkey/diff_results/ | grep j-lang | grep -v grep | wc -l`
echo ------ `uname -n` '   '$completed/$total' completed; '$ram_disk_error' RAM disk errors! '$num_diffs' diffs generated.'
