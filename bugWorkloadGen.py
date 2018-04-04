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
from shutil import copyfile
from string import maketrans


#All functions that has options go here
FallocOptions = ['FALLOC_FL_ZERO_RANGE','FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE', '0',  'FALLOC_FL_KEEP_SIZE']

FsyncOptions = ['fsync','fdatasync']

#This should take care of file name/ dir name
FileOptions = ['foo', 'bar', 'A/foo', 'A/bar']

DirOptions = ['A', 'B']

#this will take care of offset + length combo
#Start = 4-16K , append = 16K-20K, overlap = 8000 - 12096, prepend = 0-4K
WriteOptions = ['start', 'append', 'overlap', 'prepend']

#d_overlap = 8K-12K (has to be aligned)
dWriteOptions = ['start', 'append', 'd_overlap', 'prepend']

OperationSet = ['creat', 'mkdir', 'mknod', 'falloc', 'write', 'dwrite', 'link', 'unlink', 'remove', 'rename', 'symlink', 'fsetxattr', 'removexattr', 'fdatasync']

#We are skipping 041, 336, 342, 343
#The sequences we want to reach to
expected_sequence = []


#----------------------Bug summary-----------------------#

#Length 1 = 1
#Length 2 = 7
#length 3 = 5

# Total encoded = 13

#--------------------------------------------------------#


# 1. btrfs_link_unlink 3
expected_sequence.append([('link', ('foo', 'bar')), ('unlink', ('bar')), ('creat', ('bar'))])

# 2. btrfs_rename_special_file 3
expected_sequence.append([('mknod', ('foo')), ('rename', ('foo', 'bar')), ('link', ('bar', 'foo'))])

# 3. new_bug1_btrfs 2
expected_sequence.append([('write', ('foo', 'start')), ('falloc', ('foo', 'FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE', 'append'))])

# 4. new_bug2_f2fs 3
expected_sequence.append([('write', ('foo', 'start')), ('falloc', ('foo', 'FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE', 'append')), ('fdatasync', ('foo'))])

# 5. generic_034 2
expected_sequence.append([('creat', ('A/foo')), ('creat', ('A/bar'))])

# 6. generic_039 2
expected_sequence.append([('link', ('foo', 'bar')), ('remove', ('bar'))])

# 7. generic_059 2
expected_sequence.append([('write', ('foo', 'start')), ('falloc', ('foo', 'FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE', 'overlap'))])

# 8. generic_066 2
expected_sequence.append([('fsetxattr', ('foo')), ('removexattr', ('foo'))])

# 9. generic_341 3
expected_sequence.append([('creat', ('A/foo')), ('rename', ('A', 'B')), ('mkdir', ('A'))])

# 10. generic_348 1
expected_sequence.append([('symlink', ('foo', 'A/bar'))])

# 11. generic_376 2
expected_sequence.append([('rename', ('foo', 'bar')), ('creat', ('foo'))])

# 12. generic_468 3
expected_sequence.append([('write', ('foo', 'start')), ('falloc', ('foo', 'FALLOC_FL_KEEP_SIZE', 'append')), ('fdatasync', ('foo'))])

# 13. ext4_direct_write 2
expected_sequence.append([('write', ('foo', 'start')), ('dwrite', ('foo', 'prepend'))])


def build_parser():
    parser = argparse.ArgumentParser(description='Bug Workload Generator for XFSMonkey v0.1')

    # global args
    parser.add_argument('--sequence_len', '-l', default='3', help='Number of critical ops in the bugy workload')

    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Bug Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Sequence length', parsed_args.sequence_len)
    print '\n', '='*48, '\n'



def buildTuple(command):
    if command == 'creat':
        d = tuple(FileOptions)
    elif command == 'mkdir':
        d = tuple(DirOptions)
    elif command == 'mknod':
        d = tuple(FileOptions)
    elif command == 'falloc':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(FallocOptions)
        d_tmp.append(WriteOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'write':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(WriteOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'dwrite':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(dWriteOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'link' or command == 'symlink':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(FileOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
    elif command == 'rename':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(FileOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
        d_tmp = list()
        d_tmp.append(DirOptions)
        d_tmp.append(DirOptions)
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
    elif command == 'remove' or command == 'unlink':
        d = tuple(FileOptions + DirOptions)
    elif command == 'fdatasync' or command == 'fsetxattr' or command == 'removexattr':
        d = tuple(FileOptions)
    else:
        d=()
    return d



def isBugWorkload(opList, paramList):
    for i in xrange(0,len(expected_sequence)):
        if len(opList) != len(expected_sequence[i]):
            continue
        
        flag = 1
        
        for j in xrange(0, len(expected_sequence[i])):
            if opList[j] == expected_sequence[i][j][0] and paramList[j] == expected_sequence[i][j][1]:
                continue
            else:
                flag = 0
                break
    
        if flag == 1:
            print 'found match : '
            print 'Length of seq : ',  len(expected_sequence[i])
            print 'Expected sequence = ' , expected_sequence[i]
            print 'Auto generator found : '
            print opList
            print paramList
            print '\n\n'
            return True



def main():
    
    #open log file
    log_file = time.strftime('%Y%m%d_%H%M%S') + '-bugWorkloadGen.log'
    log_file_handle = open(log_file, 'w')
    
    #Parse input args
    parsed_args = build_parser().parse_args()
    
    #Print the test setup - just for sanity
    print_setup(parsed_args)
    
    num_ops = parsed_args.sequence_len

    for i in xrange(0,len(expected_sequence)):
        print 'Bug #', i
        print expected_sequence[i]
        print '\n'

    global_count = 0
    parameterList = {}
    for i in OperationSet:
        parameterList[i] = buildTuple(i)
        log = '{0}'.format(parameterList[i]);
        log = `i` + ' : Options = ' + `len(parameterList[i])` + '\n' + log + '\n\n'
        log_file_handle.write(log)

    count = 0
    permutations = []
#    for i in itertools.permutations(OperationSet, int(num_ops)):
    for i in itertools.product(OperationSet, repeat=int(num_ops)):
        permutations.append(i)
        log = ', '.join(i);
        log = `count` + ' : ' + log + '\n'
        count +=1
        global_count +=1
        log_file_handle.write(log)

        #Now for each of this permutation, find all possible permutation of paramters
        combination = list()
        for length in xrange(0,len(permutations[count-1])):
            combination.append(parameterList[permutations[count-1][length]])
        count_param = 0
        for j in itertools.product(*combination):
            log = '{0}'.format(j);
            log = `count_param` + ' : ' + log + '\n'
            count_param += 1
            global_count +=1
            log_file_handle.write(log)
            isBugWorkload(permutations[count-1], j)
#            print 'Testing is bug workload : '
#            print permutations[count-1]
#            print j




    print 'Total permutations = ', count

    print 'Total count = ' , global_count

    log_file_handle.close()


if __name__ == '__main__':
	main()
