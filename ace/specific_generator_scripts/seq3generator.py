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

FallocOptions = ['FALLOC_FL_ZERO_RANGE', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_KEEP_SIZE', 0]

FsyncOptions = ['fsync','fdatasync']

#This should take care of file name/ dir name
FileOptions = ['B/foo', 'A/foo']

SecondFileOptions = ['B/bar', 'A/bar']

#A, B are  subdir under test
DirOptions = ['A']
TestDirOptions = ['test']
SecondDirOptions = ['B']

#this will take care of offset + length combo
#Start = 4-16K , append = 16K-20K, overlap = 8000 - 12096, prepend = 0-4K

#Append should append to file size, and overwrites should be possible
#WriteOptions = ['start', 'append', 'overlap', 'prepend'] 'overlap_aligned'
WriteOptions = ['append', 'overlap_unaligned']


#d_overlap = 8K-12K (has to be aligned)
dWriteOptions = ['append', 'overlap']

#Truncate file options 'aligned'
TruncateOptions = ['unaligned']

#removed symlink, mknod
#OperationSet = ['creat', 'mkdir', 'falloc', 'write', 'dwrite', 'link', 'unlink', 'remove', 'rename', 'removexattr', 'fdatasync', 'fsetxattr', 'truncate', 'mmapwrite']

OperationSet = ['write', 'link', 'unlink', 'rename', 'truncate']
#OperationSet = ['link','rename']

#We are skipping 041, 336, 342, 343
#The sequences we want to reach to
expected_sequence = []
expected_sync_sequence = []



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
    elif file == 'A' :
        return 'B'
    elif file == 'B':
        return 'A'
    elif file == 'test':
        return 'test'

def Parent(file):
    if file == 'foo' or file == 'bar':
        return 'test'
    if file == 'A/foo' or file == 'A/bar':
        return 'A'
    if file == 'B/foo' or file == 'B/bar':
        return 'B'
    if file == 'A' or file == 'B' or file == 'test':
        return 'test'

def file_range(file_list):
    file_set = list(file_list)
    for i in xrange(0, len(file_list)):
        file_set.append(SiblingOf(file_list[i]))
        file_set.append(Parent(file_list[i]))
    return list(set(file_set))


#----------------------Bug summary-----------------------#

#Length 1 = 1
#Length 2 = 7
#length 3 = 5

# Total encoded = 13

#--------------------------------------------------------#

#If we don't allow dependency ops on same file, we'll miss this in seq2
#This is actually seq 2 = [link foo-bar, 'sync', unlink bar, 'fsync-bar']
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

#We miss this in seq-2, because we disallow workloads of sort creat, creat
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

#Reachable from current seq 2 generator  (#1360 : creat A/foo, rename A,B) (sync, fsync A)
#We will miss this, if we restrict that op2 reuses files from op1
# 9. generic_341 3 (Yes finds in 2)
expected_sequence.append([('creat', ('A/foo')), ('rename', ('A', 'B')), ('mkdir', ('A'))])
expected_sync_sequence.append([('sync'), ('none'), ('fsync', 'A')])

# 10. generic_348 1 (yes finds in 1)
expected_sequence.append([('symlink', ('foo', 'A/bar'))])
expected_sync_sequence.append([('fsync', 'A')])

# 11. generic_376 2 (yes finds in 2)
expected_sequence.append([('rename', ('foo', 'bar')), ('creat', ('foo'))])
expected_sync_sequence.append([('none'), ('fsync', 'bar')])

#Yes reachable from sseeq2 - (falloc (foo, append), fdatasync foo)
# 12. generic_468 3 (yes, finds in 2)
expected_sequence.append([('write', ('foo', 'append')), ('falloc', ('foo', 'FALLOC_FL_KEEP_SIZE', 'append')), ('fdatasync', ('foo'))])
expected_sync_sequence.append([('sync'), ('none'), ('none')])

#We miss this if we sync only used file set - or we need an option 'none' to end the file with
# 13. ext4_direct_write 2
expected_sequence.append([('write', ('foo', 'append')), ('dwrite', ('foo', 'overlap'))])
expected_sync_sequence.append([('none'), ('fsync', 'bar')])

#14 btrfs_EEXIST (Seq 1)
#creat foo, fsync foo
#write foo 0-4K, fsync foo

#btrfs use -O extref during mkfs
#15. generic 041 (If we consider the 3000 as setup, then seq length 3)
#create 3000 link(foo, foo_i), sync, unlink(foo_0), link(foo, foo_3001), link(foo, foo_0), fsync foo

#16. generic 056 (seq2)
#write(foo, 0-4K), fsync foo, link(foo, bar), fsync some random file/dir

#requires that we allow repeated operations (check if mmap write works here)
#17 generic 090 (seq3)
#write(foo 0-4K), sync, link(foo, bar), sync, append(foo, 4K-8K), fsync foo

#18 generic_104 (seq2) larger file set
#link(foo, foo1), link(bar, bar1), fsync(bar)

#19 generic 106 (seq 2)
#link(foo, bar), sync, unlink(bar) *drop cache* fsync foo

#20 generic 107 (seq 3)
#link(foo, A/foo), link(foo, A/bar), sync, unlink(A/bar), fsync(foo)

#21 generic 177
#write(foo, 0-32K), sync, punch_hole(foo, 24K-32K), punch_hole(foo, 4K-64K) fsync foo

#22 generic 321 2 fsyncs?
#rename(foo, A/foo), fsync A, fsync A/foo

#23 generic 322 (yes, seq1)
#rename(A/foo, A/bar), fsync(A/bar)

#24 generic 335 (seq 2) but larger file set
#rename(A/foo, foo), creat bar, fsync(test)

#25 generic 336 (seq 4)
#link(A/foo, B/foo), creat B/bar, sync, unlink(B/foo), mv(B/bar, C/bar), fsync A/foo


#26 generic 342 (seq 3)
# write foo 0-4K, sync, rename(foo,bar), write(foo) fsync(foo)

#27 generic 343 (seq 2)
#link(A/foo, A/bar) , rename(B/foo_new, A/foo_new), fsync(A/foo)

#28 generic 325 (seq3)
#write,(foo, 0-256K), mmapwrite(0-4K), mmapwrite(252-256K), msync(0-64K), msync(192-256K)

#29 new btrfs link (seq1)
#link(foo, bar), fsync(foo)

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

min = 0

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
        d_tmp.append(SecondFileOptions + FileOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            if len(set(i)) == 2:
                d.append(i)
    elif command == 'rename':
        d_tmp = list()
        d_tmp.append(FileOptions + SecondFileOptions)
        d_tmp.append(SecondFileOptions)
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
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    elif command == 'mmapwrite':
        d_tmp = list()
        d_tmp.append(FileOptions)
        d_tmp.append(dWriteOptions)
        d = list()
        for i in itertools.product(*d_tmp):
            d.append(i)
    else:
        d=()
    return d


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

#Creat : file should not exist. If it does, remove it.
def checkCreatDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    
    
    #Either open or closed doesn't matter. File should not exist at all
    if file_name in open_file_map:
        #Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertUnlink(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos

def checkDirDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    if file_name not in DirOptions and file_name not in SecondDirOptions:
        print 'Invalid param list for mkdir'
    
    #Either open or closed doesn't matter. File should not exist at all
    if file_name in open_dir_map and file_name != 'test':
        #if dir is A, remove contents within it too
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


        #Insert dependency before the creat command
        modified_sequence.insert(modified_pos, insertRmdir(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
            
    return modified_pos


def checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_names = current_sequence[pos][1]
    if isinstance(file_names, basestring):
        file_name = file_names
        #Parent dir doesn't exist
        if (Parent(file_name) == 'A' or Parent(file_name) == 'B')  and Parent(file_name) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1

    else:
        file_name = file_names[0]
        file_name2 = file_names[1]
        
        #Parent dir doesn't exist
        if (Parent(file_name) == 'A' or Parent(file_name) == 'B')  and Parent(file_name) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1

        #Parent dir doesn't exist
        if (Parent(file_name2) == 'A' or Parent(file_name2) == 'B')  and Parent(file_name2) not in open_dir_map:
            modified_sequence.insert(modified_pos, insertMkdir(Parent(file_name2), open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
                
    return modified_pos


# Check the dependency that file already exists and is open
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
    


    if file_name not in open_file_map or open_file_map[file_name] == 0:
        #Insert dependency - open before the command
        modified_sequence.insert(modified_pos, insertOpen(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
            
    return modified_pos


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

def checkXattr(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map):
    file_name = current_sequence[pos][1]
    if open_file_map[file_name] == 1:
        modified_sequence.insert(modified_pos, insertXattr(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
        modified_pos += 1
    return modified_pos

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
        
        #if file doesn't exist, has to be created and opened
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        #Whatever the op is, let's ensure file size is non zero
        modified_pos = checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)


    elif command == 'write' or command == 'dwrite' or command == 'mmapwrite':
        file = current_sequence[pos][1][0]
        option = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        #if file doesn't exist, has to be created and opened
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        
        #if we chose to do an append, let's not care about the file size
        # however if its an overwrite or unaligned write, then ensure file is atleast one page long
        if option == 'append':
            if file not in file_length_map:
                file_length_map[file] = 0
            file_length_map[file] += 1
        elif option == 'overlap' or 'overlap_aligned' or 'overlap_unaligned':
            modified_pos = checkFileLength(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)

        #If we do a dwrite, let's close the file after that
        if command == 'dwrite':
            if file in FileOptions or file in SecondFileOptions:
                open_file_map[file] = 0


    elif command == 'link':
        second_file = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        if second_file in open_file_map and open_file_map[second_file] == 1:
        #Insert dependency - open before the command
            modified_sequence.insert(modified_pos, insertClose(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
    
        #if we have a closed file, remove it
        if second_file in open_file_map and open_file_map[second_file] == 0:
            #Insert dependency - open before the command
            modified_sequence.insert(modified_pos, insertUnlink(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        
        #We have created a new file, but it isn't open yet
        open_file_map[second_file] = 0
    
    elif command == 'rename':
        #If the file was open during rename, does the handle now point to new file?
        first_file = current_sequence[pos][1][0]
        second_file = current_sequence[pos][1][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        #Checks if first file is closed
        modified_pos = checkClosed(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        if second_file in open_file_map and open_file_map[second_file] == 1:
            #Insert dependency - close the second file
            modified_sequence.insert(modified_pos, insertClose(second_file, open_dir_map, open_file_map, file_length_map, modified_pos))
            modified_pos += 1
        
        #We have removed the first file, and created a second file
        if first_file in FileOptions or first_file in SecondFileOptions:
            open_file_map.pop(first_file, None)
            open_file_map[second_file] = 0
        elif first_file in DirOptions or first_file in SecondDirOptions:
            open_dir_map.pop(first_file, None)
            open_dir_map[second_file] = 0
        

    elif command == 'symlink':
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        #No dependency checks
        pass
    
    elif command == 'remove' or command == 'unlink':
        #Close any open file handle and then unlink
        file = current_sequence[pos][1]
        
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos,open_dir_map, open_file_map, file_length_map)
        modified_pos = checkClosed(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        #Remove file from map
        open_file_map.pop(file, None)


    elif command == 'removexattr':
        #Check that file exists
        modified_pos = checkParentExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        
        modified_pos = checkExistsDep(current_sequence, pos, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
        #setxattr
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
            len = '4096'
            length_map[file] += 4096
        elif write_op == 'overlap_aligned':
            off = '0'
            len = '4096'
        else:
            off = '1000'
            len = '3000'

        command_str = command_str + off + ' ' + len

    if command == 'write':
        file = flat_list[1]
        write_op = flat_list[2]
        command_str = command_str + 'write ' + file.replace('/','') + ' '
        if write_op == 'append':
            len = '4096'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            
            length_map[file] += 4096

        elif write_op == 'overlap_aligned':
            off = '0'
            len = '4096'

        else:
            off = '1000'
            len = '3000'

        command_str = command_str + off + ' ' + len

    if command == 'dwrite':
        file = flat_list[1]
        write_op = flat_list[2]
        command_str = command_str + 'dwrite ' + file.replace('/','') + ' '
        
        if write_op == 'append':
            len = '4096'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            length_map[file] += 4096
    
        elif write_op == 'overlap':
            off = '0'
            len = '4096'

        command_str = command_str + off + ' ' + len

    if command == 'mmapwrite':
        file = flat_list[1]
        write_op = flat_list[2]
        ret = flat_list[3]
        command_str = command_str + 'mmapwrite ' + file.replace('/','') + ' '
        
        if write_op == 'append':
            len = '4096'
            if file not in length_map:
                length_map[file] = 0
                off = '0'
            else:
                off = str(length_map[file])
            length_map[file] += 4096

        elif write_op == 'overlap':
            off = '0'
            len = '4096'
        
        command_str = command_str + off + ' ' + len + '\ncheckpoint ' + ret

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



def doPermutation(perm):
    
    global global_count
    global parameterList
    global num_ops
    global syncPermutations
    global count
    global permutations
    global SyncSet
    global log_file_handle
    global count_param
    
    #if our permutation is of the type (write, write, write - lets skip it.) We'll handle it in write intensive workload set
    if len(set(perm)) == 1 and list(set(perm))[0] == 'write':
        return

    permutations.append(perm)
    log = ', '.join(perm);
    log = '\n' + `count` + ' : ' + log + '\n'
    count +=1
#    global_count +=1
    log_file_handle.write(log)
        
    #Now for each of this permutation, find all possible permutation of paramters
    combination = list()
    for length in xrange(0,len(permutations[count-1])):
        combination.append(parameterList[permutations[count-1][length]])
    count_param = 0
    for j in itertools.product(*combination):
        log = '{0}'.format(j)
        log = '\t' + `count_param` + ' : ' + log + '\n'
        count_param += 1
        log_file_handle.write(log)
        
        #Let's insert fsync combinations here.
        count_sync = 0
        usedFiles = list()
        flat_used_list = flatList(j)
        for file_len in xrange(0, len(flat_used_list)):
            if isinstance(flat_used_list[file_len], basestring):
                usedFilesList = list(set(flat_used_list) & set(FileOptions + SecondFileOptions + DirOptions + SecondDirOptions + TestDirOptions))
                usedFiles.append(tuple(usedFilesList))
            else:
                usedFilesList = [filter(lambda x: x in list(FileOptions + SecondFileOptions + DirOptions + SecondDirOptions + TestDirOptions), sublist) for sublist in j]
                usedFilesList = flatList(usedFilesList)
#                usedFiles.append(list(itertools.chain.from_iterable(usedFiles)))

        usedFiles = flatList(set(usedFiles))

        #TODO: to generate remaining files, use the custom set
#        syncPermutationsCustom = buildCustomTuple(file_range(usedFiles))
        syncPermutationsCustom = buildCustomTuple(usedFiles)
#        syncPermutationsCustom = [x for x in syncPermutationsCustomAll if x not in syncPermutationsCustomUsed]

        log = '\n\t\tUsed Files = {0}\n'.format(usedFiles)
        log = log + '\t\tFile range = {0}\n'.format(file_range(usedFiles))
        log_file_handle.write(log)



        isFadatasync = False
        for insSync in range(0, len(syncPermutationsCustom)):
            

            if int(num_ops) == 1 or int(num_ops) == 2 or int(num_ops) == 3 or int(num_ops) == 4:
                log = '{0}'.format(syncPermutationsCustom[insSync]);
                log = '\n\t\tFile # ' + `global_count` + ' : ' + `count_sync` + ' : ' + log + '\n'
                log_file_handle.write(log)
            global_count +=1
            count_sync+=1
            seq = []
            
            #merge the lists here . Just check if perm has fdatasync. If so skip adding any sync:
            for length in xrange(0, len(perm)):
                skip_sync = False
                op = list()
                if perm[length] == 'fdatasync' or perm[length] == 'mmapwrite':
                    skip_sync = True
                    isFadatasync = True
                else:
                    op.append(perm[length])

                #Now merge parameters
                if skip_sync:
#                    fdatasync_op = list()
                    op.append(perm[length])
                    op.append(j[length])
                    if length == len(perm)-1:
                        op.append('1')
                    else:
                        op.append('0')
                    op = tuple(flatList(op))

                else:
                    op.append(j[length])
                
                
                seq.append(tuple(op))

                if not skip_sync:
                    sync_op = list()
                    sync_op.append(syncPermutationsCustom[insSync][length])
                    if length == len(perm)-1:
                        sync_op.append('1')
                    else:
                        sync_op.append('0')
                    seq.append(tuple(flatList(sync_op)))

            log = '\t\t\tCurrent Sequence = {0}'.format(seq);
            log_file_handle.write(log)

#
##            #------Satisy dependencies now----------
#
#            modified_pos = 0
#            modified_sequence = list(seq)
#            open_file_map = {}
#            file_length_map = {}
#            open_dir_map = {}
#            #test dir exists
#            open_dir_map['test'] = 0
#            
#            for i in xrange(0, len(seq)):
#                modified_pos = satisfyDep(seq, i, modified_sequence, modified_pos, open_dir_map, open_file_map, file_length_map)
#                modified_pos += 1
#        
#            #now close all open files
#            for file_name in open_file_map:
#                if open_file_map[file_name] == 1:
#                    modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
#                    modified_pos += 1
#
#            for file_name in open_dir_map:
#                if open_dir_map[file_name] == 1:
#                    modified_sequence.insert(modified_pos, insertClose(file_name, open_dir_map, open_file_map, file_length_map, modified_pos))
#                    modified_pos += 1
#
##            #------Satisy dependencies now----------
#
#             
#             
###             #Now build the j-lang file------------------------------------
#            j_lang_file = 'j-lang' + str(global_count)
#            copyfile('code/tests/seq3/base-j-lang', j_lang_file)
#            length_map = {}
#
#            with open(j_lang_file, 'a') as f:
#                run_line = '\n\n# run\n'
#                f.write(run_line)
#
#                for insert in xrange(0, len(modified_sequence)):
#                    cur_line = buildJlang(modified_sequence[insert], length_map)
#                    cur_line_log = '{0}'.format(cur_line) + '\n'
#                    f.write(cur_line_log)
#
#            f.close()
#
#            exec_command = 'python workload_seq2.py -b code/tests/seq3/base.cpp -t ' + j_lang_file + ' -p code/tests/seq3/ -o ' + str(global_count)
#            subprocess.call(exec_command, shell=True)
#            #Now build the j-lang file------------------------------------
#
##             
#            log = '\n\t\t\tModified sequence = {0}\n'.format(modified_sequence);
#            log_file_handle.write(log)
##
##            isBugWorkload(permutations[count-1], j, syncPermutationsCustom[insSync])



global_count = 0
parameterList = {}
SyncSet = list()
num_ops = 0
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
    global permutations
    global SyncSet
    global log_file_handle
    global count_param
    
    #open log file
    log_file = time.strftime('%Y%m%d_%H%M%S') + '-bugWorkloadGen.log'
    log_file_handle = open(log_file, 'w')
    
    #Parse input args
    parsed_args = build_parser().parse_args()
    
    #Print the test setup - just for sanity
    print_setup(parsed_args)

    num_ops = parsed_args.sequence_len

    for i in xrange(0,len(expected_sequence)):
        print 'Bug #', i+1
        print expected_sequence[i]
        print expected_sync_sequence[i]
        print '\n'


    for i in OperationSet:
        parameterList[i] = buildTuple(i)
        log = '{0}'.format(parameterList[i]);
        log = `i` + ' : Options = ' + `len(parameterList[i])` + '\n' + log + '\n\n'
        log_file_handle.write(log)

    d = buildTuple('fsync')
    fsync = ('fsync',)
    sync = ('sync')
    none = ('none')

    for i in xrange(0, len(d)):
        tup = list(fsync)
        tup.append(d[i])
        SyncSet.append(tup)

    SyncSet.append(sync)
    SyncSet.append(none)
    SyncSet = tuple(SyncSet)
#    print SyncSet


    for i in itertools.product(SyncSet, repeat=int(num_ops)):
        syncPermutations.append(i)
#        print i


    start_time = time.time()

    for i in itertools.product(OperationSet, repeat=int(num_ops)):
#    for i in itertools.permutations(OperationSet, int(num_ops)):
        doPermutation(i)

#    pool = Pool(processes = 4)
#    pool.map(doPermutation, itertools.permutations(OperationSet, int(num_ops)))
#    pool.close()



    end_time = time.time()

    log = 'Total permutations of input op set = ' +  `count` + '\n'
    print log
    log_file_handle.write(log)

    log = 'Total workloads inspected = '  + `global_count`  + '\n'
    print log
    log_file_handle.write(log)

    log = 'Time taken to match workloads = ' + `end_time-start_time` + 'seconds\n\n'
    print log
    log_file_handle.write(log)

    log_file_handle.close()

#    subprocess.call('mv j-lang* code/tests/seq2/j-lang-files/', shell = True)


if __name__ == '__main__':
	main()
