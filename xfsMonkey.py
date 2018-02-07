#!/usr/bin/env python3
import os
import re
import sys
import stat
import subprocess
import argparse
import time


class Log(object):
    def __init__(self, *files):
        self.files = files
    def write(self, obj):
        for f in self.files:
            f.write(obj)
            f.flush() 
    def flush(self) :
        for f in self.files:
            f.flush()

def build_parser():
    parser = argparse.ArgumentParser(description='CrashMonkey Test Suite v1.0')

    # global args
    parser.add_argument('--fs_type', '-t', default='ext4', help='Filesystem on which you wish to run CrashMonkey test suite. Default = ext4')

    # crash monkey args
    parser.add_argument('--disk_size', '-e', default=204800, type=int, help='Size of disk in KB. Default = 200MB')
    parser.add_argument('--iterations', '-s', default=1000, type=int, help='Number of random crash states to test on. Default = 1000')
    parser.add_argument('--test_dev', '-d', default='/dev/cow_ram0', help='Test device. Default = /dev/cow_ram0')
    parser.add_argument('--flag_dev', '-f', default='/dev/sda', help='Flag device. Default = /dev/sda')
    
    #Requires changes to Makefile to place our xfstests into this folder by default.
    parser.add_argument('--test_path', '-u', default='build/xfsMonkeyTests/', help='Path to xfsMonkeyTests')
    return parser

def cleanup():
	#clean up umount and rmmod errors
	command = 'umount /mnt/snapshot; rmmod ./build/disk_wrapper.ko; rmmod ./build/cow_brd.ko'
	p=subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
	(out, err) = p.communicate()
	p.wait()
	print 'Done cleaning up test harness'

def print_setup(parsed_args):
	print '\n{: ^50s}'.format('xfsMonkey Test Suite v1.0\n')
	print '='*20, 'Setup' , '='*20, '\n'
	print '{0:20}  {1}'.format('Filesystem type', parsed_args.fs_type)
	print '{0:20}  {1}'.format('Disk size', parsed_args.disk_size)
	print '{0:20}  {1}'.format('Iterations per test', parsed_args.iterations)
	print '{0:20}  {1}'.format('Test device', parsed_args.test_dev)	
	print '{0:20}  {1}'.format('Flags device', parsed_args.flag_dev)	
	print '{0:20}  {1}'.format('Test path', parsed_args.test_path)	
	print '\n', '='*48, '\n'

def main():

	# Open the log file
	log_file = time.strftime('%Y%m%d_%H%M%S') + '-xfsMonkey.log'
	log_file_handle = open(log_file, 'w')
	original = sys.stdout
	sys.stdout = Log(sys.stdout, log_file_handle)

	parsed_args = build_parser().parse_args()

	#Print the test setup
	print_setup(parsed_args)

	#Assign a test num
	test_num = 0

	#Get the relative path to test directory
	xfsMonkeyTestPath = './' + parsed_args.test_path

	for filename in os.listdir(xfsMonkeyTestPath):
		if filename.endswith('.so'): 

			#Assign a snapshot file name. 
			#If we have a large number of tests in the test suite, then this might blow 
			#up space. We haven't added this yet.
			snapshot = filename.replace('.so', '') + '_' + parsed_args.fs_type

			#Get full test file path
			test_file = xfsMonkeyTestPath.replace('./build/', '') + filename

			#Build command to run c_harness 
			command = ('cd build; ./c_harness -f '+ parsed_args.flag_dev +' -d '+ 
			parsed_args.test_dev +' -t ' + parsed_args.fs_type + ' -e ' + 
			str(parsed_args.disk_size) + ' -s ' + str(parsed_args.iterations) +' -v ' + test_file + ' 2>&1')
	
			#Cleanup errors due to prev runs if any
			cleanup()


			#Print the test number
			test_num+=1
			print '\n', '-'*20, 'Test #',test_num, '-'*20

			#Run the test now
			print 'Running xfstest : ', filename.replace('.so', ''), 'as Crashmonkey standalone \nRunning...'
			
			#Sometimes we face an error connecting to socket. So let's retry one more time
			#if CM throws a error for a particular test.
			retry = 0
			while True:
				p=subprocess.Popen(command, stdout=subprocess.PIPE, shell=True)
				(output,err)=p.communicate()
				p_status=p.wait()

				# Printing the output on stdout seems too noisy. It's cleaner to have only the result
				# of each test printed. However due to the long writeback delay, it seems as though
				# the test case hung. 
				# (TODO : Add a flag in c_harness to interactively print when we wait for writeback
				# or start testing)
				res = re.sub(r'(?s).*Reordering', '\nReordering', output, flags=re.I)
				res_final = re.sub(r'==.*(?s)', '\n', res)

				#print output
				retry += 1
				if (p_status == 0 or retry == 2):
					if retry == 2 and p_status != 0 :
						print 'Could not run test : ', filename.replace('.so', '')
					else:
						print res_final	
					break
				else:
					error = re.sub(r'(?s).*error', '\nError', output, flags=re.I)
					print error
					print 'Retry running ' ,filename.replace('.so', ''), '\nRunning... '	 
			
	#Stop logging
	sys.stdout = original
	log_file_handle.close()
	print 'xfsMonkey test completed. See ', log_file ,' for test summary\n\n'

if __name__ == '__main__':
	main()