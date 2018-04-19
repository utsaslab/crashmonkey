#!/bin/bash

completed=`cat /home/pandian/projects/crashmonkey/xfsmonkey*.log | grep Test | tail -n1 | cut -d# -f2 | cut -d ' ' -f2`
total=`ls -lah /home/pandian/projects/crashmonkey/build/xfsMonkeyTests/j-lang* | grep -v grep | wc -l`
echo -------- `uname -n` '    ' $complete/$total completed.
