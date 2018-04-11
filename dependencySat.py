#!/usr/bin/env python

#To run : python bugWorkloadGen.py -n 3
import os
import re
import sys
import stat
import subprocess
import argparse
import time
import itertools
import json
import pprint
import collections
import threading

from shutil import copyfile
from string import maketrans
from multiprocessing import Pool


#All functions that has options go here
#FallocOptions = ['FALLOC_FL_ZERO_RANGE','FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE', '0',  'FALLOC_FL_KEEP_SIZE']

FallocOptions = ['FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE','FALLOC_FL_KEEP_SIZE']

FsyncOptions = ['fsync','fdatasync']

#This should take care of file name/ dir name
FileOptions = ['foo', 'bar', 'A/foo', 'A/bar']

DirOptions = ['A', 'B', 'test']

#this will take care of offset + length combo
#Start = 4-16K , append = 16K-20K, overlap = 8000 - 12096, prepend = 0-4K
#WriteOptions = ['start', 'append', 'overlap', 'prepend']
WriteOptions = ['append', 'overlap_aligned', 'overlap_unaligned']

#d_overlap = 8K-12K (has to be aligned)
#dWriteOptions = ['start', 'append', 'd_overlap', 'prepend']
dWriteOptions = ['append', 'overlap']

#removed setxattr
OperationSet = ['creat', 'mkdir', 'mknod', 'falloc', 'write', 'dwrite', 'link', 'unlink', 'remove', 'rename', 'symlink', 'removexattr', 'fdatasync', 'fsetxattr']

#We are skipping 041, 336, 342, 343
#The sequences we want to reach to
expected_sequence = []
expected_sync_sequence = []
current_sequence = [('creat', ('foo')), ('fsync', ('A/bar')), ('link' , ('A/foo', 'A/bar')), ('write', 'bar', 'overlap_aligned') ]
modified_sequence = list(current_sequence)
modified_pos = 0

#Note: In our op list we onlyy have creat, unlink, remove. No open and close. So we assume
# a file either exists or does not. And to satisfy dependencies, we'll use open(CREAT|RDWR) and close.
# and on each such open call, add file to open list, if not present. But on close, add to close list

# dict with file_name as key and 0 if exists but closed, and 1 if open
open_file_map = {}

# dict mapping a file with its current length
file_length_map = {}

#dict with directory name as key and value 1 if open and 0 if closed
open_dir_map = {}


#Dependencies to be satisfied:
#[('open', 'file_name'), ('fsync', 'file_name')]
#[('close', 'file_name')]
#[('unlink', 'file_name')]

def insertUnlink(file_name):
    global modified_pos
    open_file_map.pop(file_name, None)
    modified_pos += 1
    return ('unlink', file_name)

def insertRmdir(file_name):
    global modified_pos
    open_dir_map.pop(file_name, None)
    modified_pos += 1
    return ('rmdir', file_name)

def insertXattr(file_name):
    global modified_pos
    modified_pos += 1
    return ('fsetxattr', file_name)

def insertOpen(file_name):
    global modified_pos
    modified_pos += 1
    if file_name in FileOptions:
        open_file_map[file_name] = 1
    elif file_name in DirOptions:
        open_dir_map[file_name] = 1
    return ('open', file_name)

def insertClose(file_name):
    global modified_pos
    modified_pos += 1
    if file_name in FileOptions:
        open_file_map[file_name] = 0
    elif file_name in DirOptions:
        open_dir_map[file_name] = 0
    return ('close', file_name)

def insertWrite(file_name):
    global modified_pos
    modified_pos += 1
    if file_name not in file_length_map:
        file_length_map[file_name] = 0
    file_length_map[file_name] += 1
    return ('write', file_name, 'append')

#Creat : file should not exist. If it does, remove it.
def checkCreatDep(current_sequence, pos, modified_sequence, modified_pos):
    file_name = current_sequence[pos][1]
    if file_name not in FileOptions:
        print 'Invalid param list for Creat'
    

    #Either open or closed doesn't matter. File should not exist at all
    if file_name in open_file_map:
        #Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertUnlink(file_name))

def checkDirDep(current_sequence, pos, modified_sequence, modified_pos):
    file_name = current_sequence[pos][1]
    if file_name not in DirOptions:
        print 'Invalid param list for mkdir'
    
    #Either open or closed doesn't matter. File should not exist at all
    if file_name in open_dir_map:
        #Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertRmdir(file_name))


# Check the dependency that file already exists and is open
def checkExistsDep(current_sequence, pos, modified_sequence, modified_pos):
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
    else:
        file_name = file_names[0]

    # Because rename, link all require only the old path to exist
    if file_name not in FileOptions and file_name not in DirOptions:
        print 'Invalid param'

    if file_name not in open_file_map or open_file_map[file_name] == 0:
        #Insert dependency - open before the command
        modified_sequence.insert(modified_pos, insertOpen(file_name))


def checkClosed(current_sequence, pos, modified_sequence, modified_pos):
    file_name = current_sequence[pos][1]
    if file_name not in FileOptions:
        print 'Invalid param'

    if file_name in open_file_map and open_file_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertClose(file_name))

    if file_name in open_dir_map and open_dir_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertClose(file_name))

def checkXattr(current_sequence, pos, modified_sequence, modified_pos):
    file_name = current_sequence[pos][1]
    if file_name not in FileOptions:
        print 'Invalid param'
    
    if open_file_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertXattr(file_name))

def checkFileLength(current_sequence, pos, modified_sequence, modified_pos):
    file_name = current_sequence[pos][1]
    if file_name not in FileOptions:
        print 'Invalid param'
    
    # 0 length file
    if file_name not in file_length_map:
        modified_sequence.insert(modified_pos, insertWrite(file_name))




def build_parser():
    parser = argparse.ArgumentParser(description='Bug Workload Generator for XFSMonkey v0.1')

    # global args
    parser.add_argument('--sequence', '-s', default='', help='Number of critical ops in the bugy workload')

    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Bug Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Sequence', parsed_args.sequence)
    print '\n', '='*48, '\n'

min = 0

def satisfyDep(current_sequence, pos, modified_sequence, modified_pos):
    if isinstance(current_sequence[pos], basestring):
        command = current_sequence[pos]
    else:
        command = current_sequence[pos][0]

#    print 'Command = ', command

    if command == 'creat' or command == 'mknod':
        checkCreatDep(current_sequence, pos, modified_sequence, modified_pos)
        file = current_sequence[pos][1]
        open_file_map[file] = 1
    
    elif command == 'mkdir':
        checkDirDep(current_sequence, pos, modified_sequence, modified_pos)
        dir = current_sequence[pos][1]
        open_dir_map[dir] = 0

    elif command == 'falloc':
        file = current_sequence[pos][1][0]
        
        #if file doesn't exist, has to be created and opened
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)

        #Whatever the op is, let's ensure file size is non zero
        checkFileLength(current_sequence, pos, modified_sequence, modified_pos)



    elif command == 'write' or command == 'dwrite':
        file = current_sequence[pos][1][0]
        option = current_sequence[pos][1][1]
    
        #if file doesn't exist, has to be created and opened
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)
    
        #if we chose to do an append, let's not care about the file size
        # however if its an overwrite or unaligned write, then ensure file is atleast one page long
        if option == 'append':
            if file not in file_length_map:
                file_length_map[file] = 0
            file_length_map[file] += 1
        elif option == 'overlap' or 'overlap_aligned' or 'overlap_unaligned':
            checkFileLength(current_sequence, pos, modified_sequence, modified_pos)

    elif command == 'link':
        second_file = current_sequence[pos][1][1]
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)
        #We have created a new file, but it isn't open yet
        open_file_map[second_file] = 0

    elif command == 'rename':
        #If the file was open during rename, does the handle now point to new file?
        first_file = current_sequence[pos][1][0]
        second_file = current_sequence[pos][1][1]
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)
        #We have removed the first file, and created a second file
        open_file_map.pop(first_file, None)
        open_file_map[second_file] = 0

    elif command == 'symlink':
        #No dependency checks
        print command

    elif command == 'remove' or command == 'unlink':
        #Close any open file handle and then unlink
        file = current_sequence[pos][1][0]
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)
        checkClosed(current_sequence, pos, modified_sequence, modified_pos)
        
        #Remove file from map
        open_file_map.pop(file, None)


    elif command == 'removexattr':
        #Check that file exists
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)
        #setxattr
        checkXattr(current_sequence, pos, modified_sequence, modified_pos)

    elif command == 'fsync' or command == 'fdatasync':
        checkExistsDep(current_sequence, pos, modified_sequence, modified_pos)

    elif command == 'none':
        print 'Nothing to insert'

    else:
        print 'nothing'




num_ops = 0
log_file_handle = 0

def main():
    
    global num_ops
    global log_file_handle
    global modified_pos
    global modified_sequence
    
    #open log file
    log_file = time.strftime('%Y%m%d_%H%M%S') + '-bugWorkloadGen.log'
    log_file_handle = open(log_file, 'w')
    
    #Parse input args
    parsed_args = build_parser().parse_args()
    
    #Print the test setup - just for sanity
    print_setup(parsed_args)
    
 

    start_time = time.time()

    for i in xrange(0, len(current_sequence)):
        satisfyDep(current_sequence, i, modified_sequence, modified_pos)
        modified_pos+=1
    
    print current_sequence
    print modified_sequence



    end_time = time.time()


    print 'Time taken to match workloads = ', end_time-start_time, 'seconds\n\n'

    log_file_handle.close()


if __name__ == '__main__':
	main()
