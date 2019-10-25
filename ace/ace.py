#!/usr/bin/env python
# To run : python ace.py -l <seq_length> -n <nested : True|False> -d <demo : True|False>
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
from progressbar import *
from shutil import copyfile
from string import maketrans
from multiprocessing import Pool
from progress.bar import *


# All functions that has options go here

FallocOptions = ['FALLOC_FL_ZERO_RANGE', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_KEEP_SIZE', 0]

FsyncOptions = ['fsync','fdatasync', 'sync']

# This should take care of file name/ dir name
# Default option : test, test/A [foo, bar] , test/B [foo, bar]
# We have seperated it out into two sets, first and second, in order to eliminate duplicate workloads that differ just in terms of file names.
FileOptions = ['foo', 'A/foo'] # foo
SecondFileOptions = ['bar', 'A/bar'] # bar

# A,B are  subdirectories under test
# test directory(root) is under a separate list because we don't want to try to create/remove it in the workload. But we should be able to fsync it.
DirOptions = ['A']
TestDirOptions = ['test']
SecondDirOptions = ['B']


# this will take care of offset + length combo
# Start = 4-16K , append = 16K-20K, overlap = 8000 - 12096, prepend = 0-4K

# Append should append to file size, and overwrites should be possible
# WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend', 'overlap_unaligned_end']
WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend'] # 'overlap_unaligned_end'


# d_overlap = 8K-12K (has to be aligned)
# dWriteOptions = ['append', 'overlap_start', 'overlap_end']
dWriteOptions = ['append', 'overlap_start'] # 'overlap_end'

# Truncate file options 'aligned'
TruncateOptions = ['aligned', 'unaligned']

# Set of file-system operations to be used in test generation.
# We currently support : creat, mkdir, falloc, write, dwrite, link, unlink, remove, rename, fsetxattr, removexattr, truncate, mmapwrite, symlink, fsync, fdatasync, sync
OperationSet = ['creat', 'mkdir', 'falloc', 'write', 'dwrite','mmapwrite', 'link', 'unlink', 'remove', 'rename', 'fsetxattr', 'removexattr', 'truncate', 'fdatasync']

# The sequences we want to reach to, to reproduce known bugs.
expected_sequence = []
expected_sync_sequence = []


# return sibling of a file/directory
def SiblingOf(file):
    if file == 'foo':
        return 'bar'
    elif file == 'bar' :
        return 'foo'
    elif file == 'A/foo':
        return 'A/bar'
    elif file == 'A/bar':
        return 'A/foo'
    elif file == 'B/foo':
        return 'B/bar'
    elif file == 'B/bar' :
        return 'B/foo'
    elif file == 'AC/foo':
        return 'AC/bar'
    elif file == 'AC/bar' :
        return 'AC/foo'
    elif file == 'A' :
        return 'B'
    elif file == 'B':
        return 'A'
    elif file == 'AC' :
	return 'AC'
    elif file == 'test':
        return 'test'

# Return parent of a file/directory
def Parent(file):
    if file == 'foo' or file == 'bar':
        return 'test'
    if file == 'A/foo' or file == 'A/bar' or file == 'AC':
        return 'A'
    if file == 'B/foo' or file == 'B/bar':
        return 'B'
    if file == 'A' or file == 'B' or file == 'test':
        return 'test'
    if file == 'AC/foo' or file == 'AC/bar':
        return 'AC'


# Given a list of files, return a list of related files.
# These are optimizations to reduce the effective workload set, by persisting only related files during workload generation.
def file_range(file_list):
    file_set = list(file_list)
    for i in xrange(0, len(file_list)):
        file_set.append(SiblingOf(file_list[i]))
        file_set.append(Parent(file_list[i]))
    return list(set(file_set))


#----------------------Known Bug summary-----------------------#

# Length 1 = 3
# Length 2 = 14
# length 3 = 9

# Total encoded = 26
#--------------------------------------------------------------#
# TODO: Update this list carefully.
# If we don't allow dependency ops on same file, we'll miss this in seq2
# This is actually seq 2 = [link foo-bar, 'sync', unlink bar, 'fsync-bar']
# 1. btrfs_link_unlink 3 (yes finds in 2)
expected_sequence.append([('link', ('foo', 'bar')), ('unlink', ('bar')), ('creat', ('bar'))])
expected_sync_sequence.append([('sync'), ('none'), ('fsync', 'bar')])


# 2. btrfs_rename_special_file 3 (yes in 3)
expected_sequence.append([('mknod', ('foo')), ('rename', ('foo', 'bar')), ('link', ('bar', 'foo'))])
expected_sync_sequence.append([('fsync', 'bar'), ('none'), ('fsync', 'bar')])

# 3. new_bug1_btrfs 2 (Yes finds in 2)
expected_sequence.append([('write', ('foo', 'append')), ('falloc', ('foo', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE', 'append'))])
expected_sync_sequence.append([('fsync', 'foo'), ('fsync', 'foo')])

# 4. new_bug2_f2fs 3 (Yes finds in 2)
expected_sequence.append([('write', ('foo', 'append')), ('falloc', ('foo', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE', 'append')), ('fdatasync', ('foo'))])
expected_sync_sequence.append([('sync'), ('none'), ('none')])

# We miss this in seq-2, because we disallow workloads of sort creat, creat
# 5. generic_034 2
expected_sequence.append([('creat', ('A/foo')), ('creat', ('A/bar'))])
expected_sync_sequence.append([('sync'), ('fsync', 'A')])

# 6. generic_039 2 (Yes finds in 2)
expected_sequence.append([('link', ('foo', 'bar')), ('remove', ('bar'))])
expected_sync_sequence.append([('sync'), ('fsync', 'foo')])

# 7. generic_059 2 (yes finds in 2)
expected_sequence.append([('write', ('foo', 'append')), ('falloc', ('foo', 'FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE', 'overlap_unaligned'))])
expected_sync_sequence.append([('sync'), ('fsync', 'foo')])

# 8. generic_066 2 (Yes finds in 2)
expected_sequence.append([('fsetxattr', ('foo')), ('removexattr', ('foo'))])
expected_sync_sequence.append([('sync'), ('fsync', 'foo')])

# Reachable from current seq 2 generator  (#1360 : creat A/foo, rename A,B) (sync, fsync A)
# We will miss this, if we restrict that op2 reuses files from op1
# 9. generic_341 3 (Yes finds in 2)
expected_sequence.append([('creat', ('A/foo')), ('rename', ('A', 'B')), ('mkdir', ('A'))])
expected_sync_sequence.append([('sync'), ('none'), ('fsync', 'A')])

# 10. generic_348 1 (yes finds in 1)
expected_sequence.append([('symlink', ('foo', 'A/bar'))])
expected_sync_sequence.append([('fsync', 'A')])

# 11. generic_376 2 (yes finds in 2)
expected_sequence.append([('rename', ('foo', 'bar')), ('creat', ('foo'))])
expected_sync_sequence.append([('none'), ('fsync', 'bar')])

# Yes reachable from sseeq2 - (falloc (foo, append), fdatasync foo)
# 12. generic_468 3 (yes, finds in 2)
expected_sequence.append([('write', ('foo', 'append')), ('falloc', ('foo', 'FALLOC_FL_KEEP_SIZE', 'append')), ('fdatasync', ('foo'))])
expected_sync_sequence.append([('sync'), ('none'), ('none')])

# We miss this if we sync only used file set - or we need an option 'none' to end the file with
# 13. ext4_direct_write 2
expected_sequence.append([('write', ('foo', 'append')), ('dwrite', ('foo', 'overlap'))])
expected_sync_sequence.append([('none'), ('fsync', 'bar')])

#14 btrfs_EEXIST (Seq 1)
# creat foo, fsync foo
# write foo 0-4K, fsync foo

# btrfs use -O extref during mkfs
#15. generic 041 (If we consider the 3000 as setup, then seq length 3)
# create 3000 link(foo, foo_i), sync, unlink(foo_0), link(foo, foo_3001), link(foo, foo_0), fsync foo

#16. generic 056 (seq2)
# write(foo, 0-4K), fsync foo, link(foo, bar), fsync some random file/dir

# requires that we allow repeated operations (check if mmap write works here)
#17 generic 090 (seq3)
# write(foo 0-4K), sync, link(foo, bar), sync, append(foo, 4K-8K), fsync foo

#18 generic_104 (seq2) larger file set
# link(foo, foo1), link(bar, bar1), fsync(bar)

#19 generic 106 (seq 2)
# link(foo, bar), sync, unlink(bar) *drop cache* fsync foo

#20 generic 107 (seq 3)
# link(foo, A/foo), link(foo, A/bar), sync, unlink(A/bar), fsync(foo)

#21 generic 177
# write(foo, 0-32K), sync, punch_hole(foo, 24K-32K), punch_hole(foo, 4K-64K) fsync foo

#22 generic 321 2 fsyncs?
# rename(foo, A/foo), fsync A, fsync A/foo

#23 generic 322 (yes, seq1)
# rename(A/foo, A/bar), fsync(A/bar)

#24 generic 335 (seq 2) but larger file set
# rename(A/foo, foo), creat bar, fsync(test)

#25 generic 336 (seq 4)
# link(A/foo, B/foo), creat B/bar, sync, unlink(B/foo), mv(B/bar, C/bar), fsync A/foo


#26 generic 342 (seq 3)
# write foo 0-4K, sync, rename(foo,bar), write(foo) fsync(foo)

#27 generic 343 (seq 2)
# link(A/foo, A/bar) , rename(B/foo_new, A/foo_new), fsync(A/foo)

#28 generic 325 (seq3)
# write,(foo, 0-256K), mmapwrite(0-4K), mmapwrite(252-256K), msync(0-64K), msync(192-256K)


VALID_TEST_TYPES = ['crashmonkey', 'xfstest', 'xfstest-concise']

def build_parser():
    parser = argparse.ArgumentParser(description='Automatic Crash Explorer v0.1')

    # global args
    parser.add_argument('--sequence_len', '-l', default='3', help='Number of critical ops in the bugy workload')
    parser.add_argument('--nested', '-n', default='False', help='Add an extra level of nesting?')
    parser.add_argument('--demo', '-d', default='False', help='Create a demo workload set?')
    parser.add_argument('--test-type', '-t', default='crashmonkey', required=False, 
            help='Type of test to generate <{}>. (Default: crashmonkey)'.format("/".join(VALID_TEST_TYPES)))

    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('Automatic Crash Explorer v0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Sequence length', parsed_args.sequence_len)
    print '{0:20}  {1}'.format('Nested', parsed_args.nested)
    print '{0:20}  {1}'.format('Demo', parsed_args.demo)
    print '{0:20}  {1}'.format('Test Type', parsed_args.test_type)
    print '\n', '='*48, '\n'


# Helper to build all possible combination of parameters to a given file-system operation
def buildTuple(command, expand_combinations=True):
    if command == 'creat':
        d = tuple(FileOptions)
    elif command == 'mkdir' or command == 'rmdir':
        d = tuple(DirOptions)
    elif command == 'mknod':
        d = tuple(FileOptions)
    elif command == 'falloc':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(FallocOptions)
        d_tmp.append(WriteOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'write':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(WriteOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'dwrite':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(dWriteOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'link' or command == 'symlink':
        d_tmp = list()
        d_tmp.append(FileOptions + SecondFileOptions)
        d_tmp.append(SecondFileOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
    elif command == 'rename':
        d_tmp = list()
        d_tmp.append(FileOptions + SecondFileOptions)
        d_tmp.append(SecondFileOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
        d_tmp = list()
        d_tmp.append(DirOptions + SecondDirOptions)
        d_tmp.append(SecondDirOptions)
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
    elif command == 'remove' or command == 'unlink':
        d = tuple(FileOptions +SecondFileOptions)
    elif command == 'fdatasync' or command == 'fsetxattr' or command == 'removexattr':
        d = tuple(FileOptions)
    elif command == 'fsync':
        d = tuple(FileOptions + DirOptions + TestDirOptions +  SecondFileOptions + SecondDirOptions)
    elif command == 'truncate':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(TruncateOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'mmapwrite':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(dWriteOptions)
        if not expand_combinations: 
            return d_tmp
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    else:
        d=()
    return d


# Given a restricted list of files, this function builds all combinations of input parameters to persistence operations.
# Once the parameters to core-ops are picked, it is not required to persist a file totally unrelated to the set of used files in the workload. So we can restrict the set of files for persistence to either related files(includes the parent and siblings of files used in the workload) or further restrict it to strictly pick from the set of used files only.
# We can optionally add a persistence point after each core-FS op, except for the last one. The last core-op must be followed by a persistence op, so that we don't truncate it to a workload of lower sequence.
def buildCustomTuple(file_list):
    global num_ops
    
    d = list(file_list)
    fsync = ('fsync',)
    sync = ('sync')
    none = ('none')
    SyncSetCustom = list()
    SyncSetNoneCustom = list()
    for i in xrange(0, len(d)):
        tup = list(fsync)
        tup.append(d[i])
        SyncSetCustom.append(tuple(tup))
        SyncSetNoneCustom.append(tuple(tup))
    
    SyncSetCustom.append(sync)
    SyncSetNoneCustom.append(sync)
    SyncSetCustom.append(none)
    SyncSetCustom = tuple(SyncSetCustom)
    SyncSetNoneCustom = tuple(SyncSetNoneCustom)
    syncPermutationsCustom = list()

    if int(num_ops) == 1:
        for i in itertools.product(SyncSetNoneCustom):
            syncPermutationsCustom.append(i)

    elif int(num_ops) == 2:
        for i in itertools.product(SyncSetCustom, SyncSetNoneCustom):
            syncPermutationsCustom.append(i)

    elif int(num_ops) == 3:
        for i in itertools.product(SyncSetCustom, SyncSetCustom, SyncSetNoneCustom):
            syncPermutationsCustom.append(i)

    elif int(num_ops) == 4:
        for i in itertools.product(SyncSetCustom, SyncSetCustom, SyncSetCustom, SyncSetNoneCustom):
            syncPermutationsCustom.append(i)

    return syncPermutationsCustom


# Find the auto-generated workload that matches the necoded sequence of known bugs. This is to sanity check that Ace can indeed generate workloads to reproduce the bug, if run on appropriate kernel veersions.
def isBugWorkload(opList, paramList, syncList):
    for i in xrange(0,len(expected_sequence)):
        if len(opList) != len(expected_sequence[i]):
            continue
        
        flag = 1
        
        for j in xrange(0, len(expected_sequence[i])):
            if opList[j] == expected_sequence[i][j][0] and paramList[j] == expected_sequence[i][j][1] and tuple(syncList[j]) == tuple(expected_sync_sequence[i][j]):
                continue
            else:
                flag = 0
                break
    
        if flag == 1:
            print 'Found match to Bug # ', i+1, ' : in file # ' , global_count
            print 'Length of seq : ',  len(expected_sequence[i])
            print 'Expected sequence = ' , expected_sequence[i]
            print 'Expected sync sequence = ', expected_sync_sequence[i]
            print 'Auto generator found : '
            print opList
            print paramList
            print syncList
            print '\n\n'
            return True


# A bunch of functions to insert ops into the j-lang file.
def insertUnlink(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    open_file_map.pop(file_name, None)
    return ('unlink', file_name)

def insertRmdir(file_name,open_dir_map, open_file_map, file_length_map, modified_pos):
    open_dir_map.pop(file_name, None)
    return ('rmdir', file_name)

def insertXattr(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    return ('fsetxattr', file_name)

def insertOpen(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    if file_name in FileOptions or file_name in SecondFileOptions:
        open_file_map[file_name] = 1
    elif file_name in DirOptions or file_name in SecondDirOptions or file_name in TestDirOptions:
        open_dir_map[file_name] = 1
    return ('open', file_name)

def insertMkdir(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    if file_name in DirOptions or file_name in SecondDirOptions or file_name in TestDirOptions:
        open_dir_map[file_name] = 0
    return ('mkdir', file_name)

def insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    if file_name in FileOptions or file_name in SecondFileOptions:
        open_file_map[file_name] = 0
    elif file_name in DirOptions or file_name in SecondDirOptions or file_name in TestDirOptions:
        open_dir_map[file_name] = 0
    return ('close', file_name)

def insertWrite(file_name, open_dir_map, open_file_map, file_length_map, modified_pos):
    if file_name not in file_length_map:
        file_length_map[file_name] = 0
    file_length_map[file_name] += 1
    return ('write', (file_name, 'append'))


# Dependency checks : Creat - file should not exist. If it does, remove it.
def checkCreatDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    
    
    # Either open or closed doesn't matter. File should not exist at all
    if file_name in open_file_map:
        # Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertUnlink(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos

# Dependency checks : Mkdir
def checkDirDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    if file_name not in DirOptions and file_name not in SecondDirOptions:
        print 'Invalid param list for mkdir'
    
    # Either open or closed doesn't matter. Directory should not exist at all
    # TODO : We heavily depend on the pre-defined file list. Need to generalize it at some point.
    if file_name in open_dir_map and file_name != 'test':
        # if dir is A, remove contents within it too
        if file_name == 'A':
            if 'A/foo' in open_file_map and open_file_map['A/foo'] == 1:
                file = 'A/foo'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'A/foo' in open_file_map and open_file_map['A/foo'] == 0:
                file = 'A/foo'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            if 'A/bar' in open_file_map and open_file_map['A/bar'] == 1:
                file = 'A/bar'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'A/bar' in open_file_map and open_file_map['A/bar'] == 0:
                file = 'A/bar'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            
            if 'AC' in open_dir_map and open_dir_map['AC'] == 1:
                file = 'AC'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            if 'AC' in open_dir_map:
                if 'AC/foo' in open_file_map and open_file_map['AC/foo'] == 1:
                    file = 'AC/foo'
                    modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1
                    modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1
                elif 'AC/foo' in open_file_map and open_file_map['AC/foo'] == 0:
                    file = 'AC/foo'
                    modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1
                if 'AC/bar' in open_file_map and open_file_map['AC/bar'] == 1:
                    file = 'AC/bar'
                    modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1
                    modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1
                elif 'AC/bar' in open_file_map and open_file_map['AC/bar'] == 0:
                    file = 'AC/bar'
                    modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1

                file = 'AC'
                modified_sequence.insert(modified_pos, insertRmdir(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1


        if file_name == 'B':
            if 'B/foo' in open_file_map and open_file_map['B/foo'] == 1:
                file = 'B/foo'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'B/foo' in open_file_map and open_file_map['B/foo'] == 0:
                file = 'B/foo'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            if 'B/bar' in open_file_map and open_file_map['B/bar'] == 1:
                file = 'B/bar'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'B/bar' in open_file_map and open_file_map['B/bar'] == 0:
                file = 'B/bar'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1

        if file_name == 'AC':
            if 'AC/foo' in open_file_map and open_file_map['AC/foo'] == 1:
                file = 'AC/foo'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'AC/foo' in open_file_map and open_file_map['AC/foo'] == 0:
                file = 'AC/foo'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            if 'AC/bar' in open_file_map and open_file_map['AC/bar'] == 1:
                file = 'AC/bar'
                modified_sequence.insert(modified_pos, insertClose(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            elif 'AC/bar' in open_file_map and open_file_map['AC/bar'] == 0:
                file = 'AC/bar'
                modified_sequence.insert(modified_pos, insertUnlink(file, open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1


        # Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertRmdir(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
            
    return modified_pos

# Check if parent directories exist, if not create them.
def checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
        # Parent dir doesn't exist
        if (Parent(file_name) == 'A' or Parent(file_name) == 'B')  and Parent(file_name) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        if Parent(file_name) == 'AC' and Parent(file_name) not in open_dir_map:
            if Parent(Parent(file_name)) not in open_dir_map:
                modified_sequence.insert(modified_pos, insertMkdir(Parent(Parent(file_name)), open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
                    
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1



    else:
        file_name = file_names[0]
        file_name2 = file_names[1]
        
        # Parent dir doesn't exist
        if (Parent(file_name) == 'A' or Parent(file_name) == 'B')  and Parent(file_name) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        if Parent(file_name) == 'AC' and Parent(file_name) not in open_dir_map:
            if Parent(Parent(file_name)) not in open_dir_map:
                modified_sequence.insert(modified_pos, insertMkdir(Parent(Parent(file_name)), open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1

        # Parent dir doesn't exist
        if (Parent(file_name2) == 'A' or Parent(file_name2) == 'B')  and Parent(file_name2) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name2), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1

        if Parent(file_name2) == 'AC' and Parent(file_name2) not in open_dir_map:
            if Parent(Parent(file_name2)) not in open_dir_map:
                modified_sequence.insert(modified_pos, insertMkdir(Parent(Parent(file_name2)), open_dir_map, open_file_map, file_length_map, modified_pos))
                modified_pos += 1
            
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name2), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
    
    return modified_pos


# Check the dependency that file already exists and is open, eg. before writing to a file
def checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
    else:
        file_name = file_names[0]
    
    # If we are trying to fsync a dir, ensure it exists
    if file_name in DirOptions or file_name in SecondDirOptions or file_name in TestDirOptions:
        if file_name not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        if file_name in open_dir_map and open_dir_map[file_name] == 0:
            modified_sequence.insert(modified_pos, insertOpen(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1


    if file_name in FileOptions or file_name in SecondFileOptions:
        if file_name not in open_file_map or open_file_map[file_name] == 0:
        # Insert dependency - open before the command
            modified_sequence.insert(modified_pos, insertOpen(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
    
    return modified_pos

# Ensures that the file is closed. If not, closes it.
def checkClosed(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
    else:
        file_name = file_names[0]

    if file_name in open_file_map and open_file_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    
    if file_name in open_dir_map and open_dir_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos

# If the op is remove xattr, we need to ensure, there's atleast one associated xattr to the file
def checkXattr(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    if open_file_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertXattr(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos

# For overwrites ensure that the file is not empty.
def checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
    else:
        file_name = file_names[0]
    
    # 0 length file
    if file_name not in file_length_map:
        modified_sequence.insert(modified_pos, insertWrite(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos


# Handles satisfying dependencies, for a given core FS op
def satisfyDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    if isinstance(current_sequence[pos], basestring):
        command = current_sequence[pos]
    else:
        command = current_sequence[pos][0]
    
    #    print 'Command = ', command
    
    if command == 'creat' or command == 'mknod':
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkCreatDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        file = current_sequence[pos][1]
        open_file_map[file] = 1
    
    elif command == 'mkdir':
        modified_pos = checkDirDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        dir = current_sequence[pos][1]
        open_dir_map[dir] = 0

    elif command == 'falloc':
        file = current_sequence[pos][1][0]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # if file doesn't exist, has to be created and opened
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        # Whatever the op is, let's ensure file size is non zero
        modified_pos = checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)


    elif command == 'write' or command == 'dwrite' or command == 'mmapwrite':
        file = current_sequence[pos][1][0]
        option = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # if file doesn't exist, has to be created and opened
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        
        # if we chose to do an append, let's not care about the file size
        # however if its an overwrite or unaligned write, then ensure file is atleast one page long
        if option == 'append':
            if file not in file_length_map:
                file_length_map[file] = 0
            file_length_map[file] += 1
#       elif option == 'overlap_unaligned_start' or 'overlap_unaligned_end' or 'overlap_start' or 'overlap_end' or 'overlap_extend':
        elif option == 'overlap' or 'overlap_aligned' or 'overlap_unaligned':
            modified_pos = checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)

        # If we do a dwrite, let's close the file after that
        if command == 'dwrite':
            if file in FileOptions or file in SecondFileOptions:
                open_file_map[file] = 0


    elif command == 'link':
        second_file = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        if second_file in open_file_map and open_file_map[second_file] == 1:
        # Insert dependency - open before the command
            modified_sequence.insert(modified_pos, insertClose(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
    
        # if we have a closed file, remove it
        if second_file in open_file_map and open_file_map[second_file] == 0:
            # Insert dependency - open before the command
            modified_sequence.insert(modified_pos, insertUnlink(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        
        # We have created a new file, but it isn't open yet
        open_file_map[second_file] = 0
    
    elif command == 'rename':
        # If the file was open during rename, does the handle now point to new file?
        first_file = current_sequence[pos][1][0]
        second_file = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # Checks if first file is closed
        modified_pos = checkClosed(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        if second_file in open_file_map and open_file_map[second_file] == 1:
            # Insert dependency - close the second file
            modified_sequence.insert(modified_pos, insertClose(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        # We have removed the first file, and created a second file
        if first_file in FileOptions or first_file in SecondFileOptions:
            open_file_map.pop(first_file, None)
            open_file_map[second_file] = 0
        elif first_file in DirOptions or first_file in SecondDirOptions:
            open_dir_map.pop(first_file, None)
            open_dir_map[second_file] = 0
        

    elif command == 'symlink':
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # No dependency checks
        pass
    
    elif command == 'remove' or command == 'unlink':
        # Close any open file handle and then unlink
        file = current_sequence[pos][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos,open_dir_map, open_file_map, file_length_map)
        modified_pos = checkClosed(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # Remove file from map
        open_file_map.pop(file, None)


    elif command == 'removexattr':
        # Check that file exists
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        # setxattr
        modified_pos = checkXattr(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
    
    elif command == 'fsync' or command == 'fdatasync' or command == 'fsetxattr':
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)

    elif command == 'none' or command == 'sync':
        pass

    elif command == 'truncate':
        file = current_sequence[pos][1][0]
        option = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # if file doesn't exist, has to be created and opened
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        # Put some data into the file
        modified_pos = checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
    
    else:
        print command
        print 'Invalid command'

    return modified_pos

# Helper to merge lists
def flatList(op_list):
    flat_list = list()
    if not isinstance(op_list, basestring):
        for sublist in op_list:
            if not isinstance(sublist, basestring):
                for item in sublist:
                    flat_list.append(item)
            else:
                flat_list.append(sublist)
    else:
        flat_list.append(op_list)

    return flat_list

# Returns a list of lines to output into J2lang file.
def buildJ2lang(sequence):
    # Python2 does not support nonlocal variables,
    # workaround by using dictionary.
    d = dict(lines=["# J2-Lang\n"], file_num=1, options_num=1)

    def add_line(line):
        d['lines'].append(line + "\n")

    def new_file(options):
        fname = "file" + str(d['file_num'])
        d['file_num'] += 1

        options = list(map(lambda f: f.replace('/', ''), options))
        add_line("{} {}".format(fname, " ".join(options)))
        return fname

    def new_option(options):
        optname = "option" + str(d['options_num'])
        d['options_num'] += 1

        options = list(map(str, options))
        add_line("{} {}".format(optname, " ".join(options)))
        return optname

    for op, parameters in sequence:
        if op == "creat":
            f1 = new_file(parameters[0])
            add_line("open ${} O_RDWR|O_CREAT 0777".format(f1))
        elif op == "mkdir":
            f1 = new_file(parameters[0])
            add_line("mkdir ${} 0777".format(f1))
        elif op == "falloc":
            f1 = new_file(parameters[0])
            op1 = new_option(parameters[1])
            op2 = new_option(parameters[2])

            add_line("falloc ${} ${} ${}".format(f1, op1, op2))
        elif op == "write":
            f1 = new_file(parameters[0])
            op1 = new_option(parameters[1])

            add_line("write ${} ${}".format(f1, op1))
        elif op == "dwrite":
            f1 = new_file(parameters[0])
            op1 = new_option(parameters[1])

            add_line("dwrite ${} ${}".format(f1, op1))
        elif op == "mmapwrite":
            f1 = new_file(parameters[0])
            op1 = new_option(parameters[1])

            add_line("mmapwrite ${} ${}".format(f1, op1))
        elif (op == "link" or op == "rename"):
            f1, f2 = new_file(parameters[0]), new_file(parameters[1])
            add_line("{} ${} ${}".format(op, f1, f2))
        elif (op == "unlink" or op == "remove" or op == "fsetxattr" or op == "removexattr" or op == "fdatasync"):
            f1 = new_file(parameters[0])
            add_line("{} ${}".format(op, f1))
        elif op == "truncate":
            def map_truncate_option(opt):
                if opt == "aligned": return "0"
                else: return "2500"

            f1 = new_file(parameters[0])
            opt = new_option(list(map(map_truncate_option, parameters[1])))
            add_line("truncate ${} ${}".format(f1, opt))
        else:
            raise ValueError("Operation '{}' with parameters {} is unsupported".format(op, parameters))

    return d['lines']


# Creates the actual J-lang file.
def buildJlang(op_list, length_map):
    flat_list = list()
    if not isinstance(op_list, basestring):
        for sublist in op_list:
            if not isinstance(sublist, basestring):
                for item in sublist:
                    flat_list.append(item)
            else:
                flat_list.append(sublist)
    else:
        flat_list.append(op_list)

    command_str = ''
    command = flat_list[0]
    if command == 'open':
        file = flat_list[1]
        if file in DirOptions or file in SecondDirOptions or file in TestDirOptions:
            command_str = command_str + 'opendir ' + file.replace('/','') + ' 0777'
        else:
            command_str = command_str + 'open ' + file.replace('/','') + ' O_RDWR|O_CREAT 0777'

    if command == 'creat':
        file = flat_list[1]
        command_str = command_str + 'open ' + file.replace('/','') + ' O_RDWR|O_CREAT 0777'

    if command == 'mkdir':
        file = flat_list[1]
        command_str = command_str + 'mkdir ' + file.replace('/','') + ' 0777'

    if command == 'mknod':
        file = flat_list[1]
        command_str = command_str + 'mknod ' + file.replace('/','') + ' TEST_FILE_PERMS|S_IFCHR|S_IFBLK' + ' 0'


    if command == 'falloc':
        file = flat_list[1]
        option = flat_list[2]
        write_op = flat_list[3]
        command_str = command_str + 'falloc ' + file.replace('/','') + ' ' + str(option) + ' '
        if write_op == 'append':
            off = str(length_map[file])
            lenn = '32768'
            length_map[file] += 32768
        elif write_op == 'overlap_unaligned_start':
            off = '0'
            lenn = '5000'
        elif write_op == 'overlap_unaligned_end':
            size = length_map[file]
            off = str(size-5000)
            lenn = '5000'
        elif write_op == 'overlap_extend':
            size = length_map[file]
            off = str(size-2000)
            lenn = '5000'
            length_map[file] += 3000
        
        command_str = command_str + off + ' ' + lenn

    if command == 'write':
        file = flat_list[1]
        write_op = flat_list[2]
        command_str = command_str + 'write ' + file.replace('/','') + ' '
        if write_op == 'append':
            lenn = '32768'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            
            length_map[file] += 32768
        
        elif write_op == 'overlap_unaligned_start':
            off = '0'
            lenn = '5000'
        elif write_op == 'overlap_unaligned_end':
            size = length_map[file]
            off = str(size-5000)
            lenn = '5000'
        elif write_op == 'overlap_extend':
            size = length_map[file]
            off = str(size-2000)
            lenn = '5000'
        
        command_str = command_str + off + ' ' + lenn

    if command == 'dwrite':
        file = flat_list[1]
        write_op = flat_list[2]
        command_str = command_str + 'dwrite ' + file.replace('/','') + ' '
        
        if write_op == 'append':
            lenn = '32768'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            length_map[file] += 32768

        elif write_op == 'overlap_start':
            off = '0'
            lenn = '8192'
        elif write_op == 'overlap_end':
            size = length_map[file]
            off = str(size-8192)
            lenn = '8192'

        command_str = command_str + off + ' ' + lenn
    
    if command == 'mmapwrite':
        file = flat_list[1]
        write_op = flat_list[2]
        ret = flat_list[3]
        command_str = command_str + 'mmapwrite ' + file.replace('/','') + ' '
        
        if write_op == 'append':
            lenn = '32768'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            length_map[file] += 32768
        
        elif write_op == 'overlap_start':
            off = '0'
            lenn = '8192'
        elif write_op == 'overlap_end':
            size = length_map[file]
            off = str(size-8192)
            lenn = '8192'
        
        command_str = command_str + off + ' ' + lenn + '\ncheckpoint ' + ret

    

    if command == 'link' or command =='rename' or command == 'symlink':
        file1 = flat_list[1]
        file2 = flat_list[2]
        command_str = command_str + command + ' ' + file1.replace('/','') + ' ' + file2.replace('/','')

    if command == 'unlink'or command == 'remove' or command == 'rmdir' or command == 'close' or command == 'fsetxattr' or command == 'removexattr':
        file = flat_list[1]
        command_str = command_str + command + ' ' + file.replace('/','')

    if command == 'fsync':
        file = flat_list[1]
        ret = flat_list[2]
        command_str = command_str + command + ' ' + file.replace('/','') + '\ncheckpoint ' + ret

    if command =='fdatasync':
        file = flat_list[1]
        ret = flat_list[2]
        command_str = command_str + command + ' ' + file.replace('/','') + '\ncheckpoint ' + ret


    if command == 'sync':
        ret = flat_list[1]
        command_str = command_str + command + '\ncheckpoint ' + ret

    if command == 'none':
        command_str = command_str + command


    if command == 'truncate':
        file = flat_list[1]
        trunc_op = flat_list[2]
        command_str = command_str + command + ' ' + file.replace('/','') + ' '
        if trunc_op == 'aligned':
            len = '0'
            length_map[file] = 0
        elif trunc_op == 'unaligned':
            len = '2500'
        command_str = command_str + len

    return command_str

# Main function that exhaustively generates combinations of ops.
def doPermutation(perm, test_type):
    
    global global_count
    global parameterList
    global num_ops
    global nested
    global demo
    global syncPermutations
    global count
    global permutations
    global SyncSet
    global log_file_handle
    global count_param
    
    dest_dir = 'seq'+num_ops
    
    if nested:
        dest_dir += '_nested'
    
    if demo:
        dest_dir += '_demo'

    
    # Hacks to reduce the workload set (#1):
    # Eliminate workloads of seq-3 in which all three core-ops are write, because we have a write specific workload generator to explore these cases.
    if int(num_ops)==3 and len(set(perm)) == 1 and list(set(perm))[0] == 'write':
        return

    permutations.append(perm)
    log = ', '.join(perm);
    log = '\n' + `count` + ' : ' + log + '\n'
    count +=1
    log_file_handle.write(log)
        
    # Now for each of this permutation of file-system operations, find all possible permutation of paramters
    combination = list()
    for length in xrange(0,len(permutations[count-1])):
        combination.append(parameterList[permutations[count-1][length]])

    count_param = 0

    # **PHASE 2** : Exhaustively populate parameters to the chosen skeleton in phase 1
    for currentParameterOption in itertools.product(*combination):
        
        # files used so far
        usedSofar = list()
        intersect = list()
        
        # Hacks to reduce the workload set (#2):
        # Allow this combination of parameters only if we reuse files across the chosen operations.
        toSkip = False
        for paramLength in xrange(0, int(num_ops)):
            intersect = list(set(currentParameterOption[paramLength]) & set(usedSofar))
            if currentParameterOption[paramLength][0] == 'A' or currentParameterOption[paramLength][0] == 'B' or currentParameterOption[paramLength][0] == 'AC':
                intersect.append('A')
            elif len(usedSofar) == 2 and (usedSofar[0] == 'A' or  usedSofar[0] == 'B' or usedSofar[0] == 'AC'):
                intersect.append('A')
            usedSofar = list(set(currentParameterOption[paramLength]) | set(usedSofar))
            if len(intersect) == 0 and paramLength > 0 and int(num_ops) == 3:
                # print 'Skip this option'
                toSkip = True
                continue

        if toSkip:
            continue

        log = '{0}'.format(currentParameterOption)
        log = '\t' + `count_param` + ' : ' + log + '\n'
        count_param += 1
        log_file_handle.write(log)
        
        # Let's insert fsync combinations here.
        count_sync = 0
        usedFiles = list()
        flat_used_list = flatList(currentParameterOption)
        for file_len in xrange(0, len(flat_used_list)):
            if isinstance(flat_used_list[file_len], basestring):
                usedFilesList = list(set(flat_used_list) & set(FileOptions + SecondFileOptions + DirOptions + SecondDirOptions + TestDirOptions))
                usedFiles.append(tuple(usedFilesList))
            # else:
            #   usedFilesList = [filter(lambda x: x in list(FileOptions + SecondFileOptions + DirOptions + SecondDirOptions + TestDirOptions), sublist) for sublist in currentParameterOption]
            #   usedFilesList = flatList(usedFilesList)
            #   usedFiles = list(set(set(usedFilesList) | set(usedFiles)))

        usedFiles = flatList(set(usedFiles))

        # For lower sequences, let's allow fsync on any related file - sibling/parent
        if int(num_ops) < 3 and not demo:
            syncPermutationsCustom = buildCustomTuple(file_range(usedFiles))
        else:
            syncPermutationsCustom = buildCustomTuple(usedFiles)

        # Logging the list of used files
        log = '\n\t\tUsed Files = {0}\n'.format(usedFiles)
        log = log + '\t\tFile range = {0}\n'.format(file_range(usedFiles))
        log_file_handle.write(log)


        # Fdatasync could be used as a file-system op itself.
        # So, we need to ensure that we don't insert an additional persistence point after it.
        # Also, mmapwrite has msync attached to it. So we'll skip adding persistence point after mmapwrite as well.
        isFadatasync = False

        # **PHASE 3** : Insert all combinations of persistence ops to the generated phase-2 workloads.
        for insSync in range(0, len(syncPermutationsCustom)):
            if int(num_ops) < 4:
                log = '{0}'.format(syncPermutationsCustom[insSync]);
                log = '\n\t\tFile # ' + `global_count` + ' : ' + `count_sync` + ' : ' + log + '\n'
                log_file_handle.write(log)
            global_count +=1
            count_sync+=1
            seq = []
            
            # merge the lists here . Just check if perm has fdatasync. If so skip adding any sync:
            for length in xrange(0, len(perm)):
                skip_sync = False
                op = list()
                if perm[length] == 'fdatasync' or perm[length] == 'mmapwrite':
                    skip_sync = True
                    isFadatasync = True
                else:
                    op.append(perm[length])

                # Now merge parameters and add return value to the persistence point.
                # A CM checkpoint() is added after persistence point, which has to return 1 if its the last persistence point in the workload, else 0. We handle this CM specific requirement here.
                if skip_sync:
                    op.append(perm[length])
                    op.append(currentParameterOption[length])
                    if length == len(perm)-1:
                        op.append('1')
                    else:
                        op.append('0')
                    op = tuple(flatList(op))

                else:
                    op.append(currentParameterOption[length])
                
                
                seq.append(tuple(op))

                if not skip_sync:
                    sync_op = list()
                    sync_op.append(syncPermutationsCustom[insSync][length])
                    if length == len(perm)-1:
                        sync_op.append('1')
                    else:
                        sync_op.append('0')
                    seq.append(tuple(flatList(sync_op)))

            # Logging the skeleton of the workload, including persistence operations and paramters.
            log = '\t\t\tCurrent Sequence = {0}'.format(seq);
            log_file_handle.write(log)


            #--------------Satisy dependencies now--------------------
            # **PHASE 4** : Deterministic stage - satisfy dependencies for all ops in the list so far.
            modified_pos = 0
            modified_sequence = list(seq)
            open_file_map = {}
            file_length_map = {}
            open_dir_map = {}

            # test dir exists
            open_dir_map['test'] = 0

            # Go over the current sequence of operations and satisfy dependencies for each file-system op
            for i in xrange(0, len(seq)):
                modified_pos = satisfyDep(seq, i, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
                modified_pos += 1
        
            # now close all open files
            for file_name in open_file_map:
                if open_file_map[file_name] == 1:
                    modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1

            # close all open directories
            for file_name in open_dir_map:
                if open_dir_map[file_name] == 1:
                    modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
                    modified_pos += 1

            #--------------Now build the j-lang file-------------------
            j_lang_file = 'j-lang' + str(global_count)
            source_j_lang_file = '../code/tests/' + dest_dir + '/base-j-lang'
            copyfile(source_j_lang_file, j_lang_file)
            length_map = {}

            with open(j_lang_file, 'a') as f:
                run_line = '\n\n# run\n'
                f.write(run_line)

                for insert in xrange(0, len(modified_sequence)):
                    cur_line = buildJlang(modified_sequence[insert], length_map)
                    cur_line_log = '{0}'.format(cur_line) + '\n'
                    f.write(cur_line_log)

            f.close()

            if test_type == 'crashmonkey':
                exec_command = 'python2 cmAdapter.py -b ../code/tests/' + dest_dir + '/base.cpp -t ' + j_lang_file + ' -p ../code/tests/' + dest_dir + '/ -o ' + str(global_count)
            elif test_type == 'xfstest':
                exec_command = 'python2 xfstestAdapter.py -t ' + j_lang_file + ' -p ../code/tests/' + dest_dir + '/ -n ' + str(global_count) + " -f generic"
            subprocess.call(exec_command, shell=True)


            log = '\n\t\t\tModified sequence = {0}\n'.format(modified_sequence);
            log_file_handle.write(log)

# Exhaustively generates combinations of ops, but create j2-lang
# files that allow the xfstest adapter to create more concise
# tests.
def doPermutationV2(perm):
    
    global global_count
    global parameterList
    global num_ops
    global nested
    global demo
    global syncPermutations
    global count
    global permutations
    global SyncSet
    global log_file_handle
    global count_param
    
    dest_dir = 'seq'+num_ops
    
    if nested:
        dest_dir += '_nested'
    
    if demo:
        dest_dir += '_demo'
    
    # For now, we only support operations of length 1.  
    if len(num_ops) != 1:
        print("\nError - xfstest-concise currently only supports operations of length 1!")
        sys.exit(1)

    permutations.append(perm)
    log = ', '.join(perm);
    log = '\n' + `count` + ' : ' + log + '\n'
    count +=1
    log_file_handle.write(log)

    # Now for each of this permutation of file-system operations, find all possible parameters
    parameter_options = []
    for p in perm:
        options = buildTuple(p, expand_combinations=False)
        if isinstance(options, tuple):
            options = [options]
        parameter_options.append(options)

    seq = zip(perm, parameter_options)

    # Logging the skeleton of the workload, including persistence operations and paramters.
    log = '\t\t\tCurrent Sequence = {0}'.format(seq);
    log_file_handle.write(log)

    #--------------Now build the j2-lang file-------------------#
    global_count += 1
    j2_lang_file = 'j2-lang' + str(global_count)

    with open(j2_lang_file, 'w') as f:
        f.writelines(buildJ2lang(seq))

    exec_command = 'python2 xfstestAdapter.py -t ' + j2_lang_file + ' -p ../code/tests/' + dest_dir + ' -n ' + "{:03d}".format(global_count) + " -f generic"
    subprocess.call(exec_command, shell=True)

class SlowBar(FillingCirclesBar):
    suffix = '%(percent).0f%%  (Completed %(index)d skeletons with %(global_count)d workloads)'
    @property
    def global_count(self):
        return global_count

global_count = 0
parameterList = {}
SyncSet = list()
num_ops = 0
nested = False
demo = False
syncPermutations = []
count = 0
permutations = []
log_file_handle = 0
count_param = 0

def main():
    
    global global_count
    global parameterList
    global num_ops
    global syncPermutations
    global count
    global nested
    global permutations
    global SyncSet
    global demo
    global log_file_handle
    global count_param
    global FileOptions
    global SecondFileOptions
    global SecondDirOptions
    global OperationSet
    global FallocOptions
    
    # open log file
    log_file = time.strftime('%Y%m%d_%H%M%S') + '-bugWorkloadGen.log'
    log_file_handle = open(log_file, 'w')
    
    # Parse input args
    parsed_args = build_parser().parse_args()
    
    # Print the test setup - just for sanity
    print_setup(parsed_args)

    num_ops = parsed_args.sequence_len
    test_type = parsed_args.test_type

    if test_type not in VALID_TEST_TYPES:
        print("Invalid test type '{}'\nTest type must be one of <{}>"
                .format(test_type, "/".join(VALID_TEST_TYPES)))
        sys.exit(1)
    
    if parsed_args.nested == ('True' or 'true'):
        nested = True
    
    else:
        nested = False

    if parsed_args.demo == ('True' or 'true'):
        demo = True
        OperationSet = ['link','falloc']
        FallocOptions = ['FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE']
        FileOptions = ['foo']
        SecondFileOptions = ['A/bar']
    
    else:
        demo = False

    # In case the user requests for an additional level of nesting, add one nested dir A/C and one file within it A/C/foo.
    if nested:
        FileOptions = FileOptions + ['AC/foo']
	SecondFileOptions = SecondFileOptions + ['AC/bar']
        SecondDirOptions = SecondDirOptions + ['AC']

    # We just print all known bugs
    # for i in xrange(0,len(expected_sequence)):
    #   print 'Bug #', i+1
    #   print expected_sequence[i]
    #   print expected_sync_sequence[i]
    #   print '\n'


    # This is basically the list of possible paramter options for each file-system operation. For example, if the fileset has 4 files and the op is creat, then there are 4 parameter options to creat. We log it just to get an estimate of the increase in the options as we expand the file set.
    for i in OperationSet:
        parameterList[i] = buildTuple(i)
        log = '{0}'.format(parameterList[i]);
        log = `i` + ' : Options = ' + `len(parameterList[i])` + '\n' + log + '\n\n'
        log_file_handle.write(log)

    d = buildTuple('fsync')
    fsync = ('fsync',)
    sync = ('sync')
    none = ('none')
    fdatasync = ('fdatasync',)

    for i in xrange(0, len(d)):
        tup = list(fsync)
        tup.append(d[i])
        SyncSet.append(tup)

    for i in xrange(0, len(d)):
        tup = list(fdatasync)
        tup.append(d[i])
        SyncSet.append(tup)

    SyncSet.append(sync)
    SyncSet.append(none)
    SyncSet = tuple(SyncSet)


    dest_dir = 'seq'+num_ops
    
    if nested:
        dest_dir += '_nested'

    if demo:
        dest_dir += '_demo'
    

    # Create a target directory for the final .cpp test files
    # The corresponding high-level language file for each test case can be found wthin the directory j-lang-files in the same target directory.
    target_path = '../code/tests/' + dest_dir + '/j-lang-files/'
    if not os.path.exists(target_path):
        os.makedirs(target_path)

    # copy base files into this directory
    # We assume that a directory ace-base exists with skeleton for the base j-lang and .cpp files. This has to be something pre-written according to the format required by your record-and-replay tool.
    if test_type == "crashmonkey" or test_type == "xfstest":
        dest_j_lang_file = '../code/tests/' + dest_dir + '/base-j-lang'
        source_j_lang_file = '../code/tests/ace-base/base-j-lang'
        copyfile(source_j_lang_file, dest_j_lang_file)

    if test_type == "crashmonkey":
        dest_j_lang_cpp = '../code/tests/' + dest_dir + '/base.cpp'
        source_j_lang_cpp = '../code/tests/ace-base/base.cpp'
        copyfile(source_j_lang_cpp, dest_j_lang_cpp)

    # Create all permutations of persistence ops  of given sequence length. This will be merged to the phase-2 workload.
    for i in itertools.product(SyncSet, repeat=int(num_ops)):
        syncPermutations.append(i)

    # This is the number of input operations
    log = 'Total file-system operations tested = ' +  `len(OperationSet)` + '\n'
    print log
    log_file_handle.write(log)

    # Time workload generation
    start_time = time.time()

    # **PHASE 1** : Create all permutations of file-system operations of given sequence lengt. ops can repeat.
    # To create only permutations of ops with no repeptions allowed, use this
    # for i in itertools.permutations(OperationSet, int(num_ops)):
    totalOpCombinations = len(OperationSet) ** int(num_ops)
    # Workloads can take really long to generate. So let's create a progress bar.
    # bar = FillingCirclesBar('Generating workloads.. ', max=totalOpCombinations, suffix='%(percent).0f%%  (Completed %(index)d/%(global_count)d)')
    bar = SlowBar('Generating workloads.. ', max=totalOpCombinations)

    # This is the number of input operations
    log = 'Total Phase-1 Skeletons = ' + `totalOpCombinations` + '\n'
    if not demo:
	print log
    log_file_handle.write(log)

    bar.start()
    for i in itertools.product(OperationSet, repeat=int(num_ops)):
        if test_type == "xfstest-concise":
            doPermutationV2(i)
        else:
            doPermutation(i, test_type)
        bar.next()

    # To parallelize workload generation, we will need to modify how we number the workloads. If we enable this as it is, the threads will overwrite workloads, as they don't see the global_count.
    # pool = Pool(processes = 4)
    # pool.map(doPermutation, itertools.permutations(OperationSet, int(num_ops)))
    # pool.close()

    # End timer
    end_time = time.time()
    bar.finish()

    # This is the total number of workloads generated
    log = '\nTotal workloads generated = '  + `global_count`  + '\n'
    print log
    log_file_handle.write(log)

    # Time to create the above workloads
    log = 'Time taken to generate workloads = ' + `round(end_time-start_time,2)` + ' seconds\n'
    if not demo:
	print log
    log_file_handle.write(log)

    # Print target directory
    log = 'Generated workloads can be found at ../code/tests/' + dest_dir + '\n'
    print log
    log_file_handle.write(log)

    log_file_handle.close()


    # Move the resultant high-level lang files to target directory. We could choose to delete them too. But if we wanted to analyze the core-ops in a workload, looking at this file is an easier way of doing it. Also if you modify the adapter, you can simply supply the directory of j-lang files to convert to cpp. No need to go through the entire generation process.
    target = 'mv j2-lang* {}' if test_type == "xfstest-concise" else 'mv j-lang* {}'
    subprocess.call(target.format(target_path), shell = True)


if __name__ == '__main__':
	main()
