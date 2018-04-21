#!/usr/bin/env python

#To run : python workload.py -b code/tests/generic_039/base_test.cpp  -t code/tests/generic_039/generic_039 -p code/tests/generic_039
import os
import re
import sys
import stat
import subprocess
import argparse
import time
import itertools
from shutil import copyfile
from string import maketrans


#All functions that has options go here
FallocOptions = ['FALLOC_FL_ZERO_RANGE','FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE','FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE', '0',  'FALLOC_FL_KEEP_SIZE']

FsyncOptions = ['fsync','fdatasync']

RemoveOptions = ['remove','unlink']

LinkOptions = ['link','symlink']

WriteOptions = ['WriteData','WriteDataMmap', 'pwrite']

redeclare_map = {}

def build_parser():
    parser = argparse.ArgumentParser(description='Workload Generator for XFSMonkey v1.0')

    # global args
    parser.add_argument('--base_file', '-b', default='', help='Base test file to generate workload')
    parser.add_argument('--test_file', '-t', default='', help='J lang test skeleton to generate workload')

    # crash monkey args
    parser.add_argument('--target_path', '-p', default='code/tests/', help='Directory to save the generated test files')

    parser.add_argument('--output_name', '-o', default='file', help='Name of the generated file')
    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Base test file', parsed_args.base_file)
    print '{0:20}  {1}'.format('Test skeleton', parsed_args.test_file)
    print '{0:20}  {1}'.format('Target directory', parsed_args.target_path)
    print '{0:20}  {1}'.format('Output file', parsed_args.output_name)
    print '\n', '='*48, '\n'
    

def create_dir(dir_path):
    try: 
        os.makedirs(dir_path)
    except OSError:
        if not os.path.isdir(dir_path):
            raise

def create_dict():
    operation_map = {'fsync': 0, 'fallocate': 0, 'open': 0, 'remove': 0}
    return operation_map


#These maps keep track of the line number in each method, to add the next function to in the C++ file
def updateSetupMap(index_map, num):
    index_map['setup'] += num
    index_map['run'] += num
    index_map['check'] += num
    index_map['define'] += num

def updateRunMap(index_map, num):
    index_map['run'] += num
    index_map['check'] += num
    index_map['define'] += num

def updateCheckMap(index_map, num):
    index_map['check'] += num
    index_map['define'] += num

def updateDefineMap(index_map, num):
    index_map['define'] += num



def insertDeclare(line, file, index_map):
    
    with open(file, 'r+') as declare:
        contents = declare.readlines()

        updateRunMap(index_map, 1)
        
        to_insert = '\t\t\t\tint ' + line + ' = 0 ;\n'
        contents.insert(index_map['run'], to_insert)

        declare.seek(0)
        declare.writelines(contents)
        declare.close()


# Add the 'line' which declares a file/dir used in the workload into the 'file'
# at position specified in the 'index_map'
def insertDefine(line, file, index_map):
    with open(file, 'r+') as define:
        
        contents = define.readlines()
        
        #Initialize paths in setup phase
        updateSetupMap(index_map, 1)
        file_str = ''
        if len(line.split('/')) != 1 :
            for i in xrange(0, len(line.split('/'))):
                file_str += line.split('/')[i]
        else:
            file_str = line.split('/')[-1]
        
        if file_str == 'test':
            to_insert = '\t\t\t\t' + file_str + '_path = mnt_dir_ ;\n'
        else:
            to_insert = '\t\t\t\t' + file_str + '_path = mnt_dir_' + ' + "/' + line + '";\n'
        
        contents.insert(index_map['setup'], to_insert)
        
        #Initialize paths in run phase
        updateRunMap(index_map, 1)
        file_str = ''
        if len(line.split('/')) != 1 :
            for i in xrange(0, len(line.split('/'))):
                file_str += line.split('/')[i]
        else:
            file_str = line.split('/')[-1]

        if file_str == 'test':
            to_insert = '\t\t\t\t' + file_str + '_path = mnt_dir_ ;\n'
        else:
            to_insert = '\t\t\t\t' + file_str + '_path =  mnt_dir_' + ' + "/' + line + '";\n'
        contents.insert(index_map['run'], to_insert)
        
        #Initialize paths in check phase
        updateCheckMap(index_map, 1)
        file_str = ''
        if len(line.split('/')) != 1 :
            for i in xrange(0, len(line.split('/'))):
                file_str += line.split('/')[i]
        else:
            file_str = line.split('/')[-1]

        if file_str == 'test':
            to_insert = '\t\t\t\t' + file_str + '_path = mnt_dir_ ;\n'
        else:
            to_insert = '\t\t\t\t' + file_str + '_path =  mnt_dir_' + ' + "/' + line + '";\n'
        contents.insert(index_map['check'], to_insert)
        
        #Update defines portion
        #Get only the file name. We don't want the path here
        updateDefineMap(index_map, 1)
        file_str = ''
        if len(line.split('/')) != 1 :
            for i in xrange(0, len(line.split('/'))):
                file_str += line.split('/')[i]
        else:
            file_str = line.split('/')[-1]
        to_insert = '\t\t\t string ' + file_str + '_path; \n'

        contents.insert(index_map['define'], to_insert)
        
        define.seek(0)
        define.writelines(contents)
        define.close()


def insertFalloc(contents, line, index_map, method):

    to_insert = '\n\t\t\t\tif ( fallocate( fd_' + line.split(' ')[1] + ' , ' + line.split(' ')[2] + ' , ' + line.split(' ')[3] + ' , '  + line.split(' ')[4] + ') < 0){ \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1]  +');\n\t\t\t\t\t return errno;\n\t\t\t\t}\n\n'

    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 5)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 5)

def insertMkdir(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( mkdir(' + line.split(' ')[1] + '_path.c_str() , ' + line.split(' ')[2] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)



def insertOpenFile(contents, line, index_map, method):
    
    name = 'fd_' + line.split(' ')[1]
    decl = ' '
    if name not in redeclare_map:
        decl = 'int '
        redeclare_map[name] = 1
    
    # TODO: prevent redeclations here
    to_insert = '\n\t\t\t\t' + decl + 'fd_' + line.split(' ')[1] + ' = cm_->CmOpen(' + line.split(' ')[1] + '_path.c_str() , ' + line.split(' ')[2] + ' , ' + line.split(' ')[3] + '); \n\t\t\t\tif ( fd_' + line.split(' ')[1] + ' < 0 ) { \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 6)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 6)

def insertMknodFile(contents, line, index_map, method):
    
    name = 'fd_' + line.split(' ')[1]
    decl = ' '
    if name not in redeclare_map:
        decl = 'int '
        redeclare_map[name] = 1
    
    # TODO: prevent redeclations here
    to_insert = '\n\t\t\t\t' + decl + 'fd_' + line.split(' ')[1] + ' = mknod(' + line.split(' ')[1] + '_path.c_str() , ' + line.split(' ')[2] + ' , ' + line.split(' ')[3] + '); \n\t\t\t\tif ( fd_' + line.split(' ')[1] + ' < 0 ) { \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'

    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 6)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 6)

def insertOpenDir(contents, line, index_map, method):
    
    name = 'fd_' + line.split(' ')[1]
    decl = ' '
    if name not in redeclare_map:
        decl = 'int '
        redeclare_map[name] = 1
    
    # TODO: prevent redeclations here
    to_insert = '\n\t\t\t\t' + decl + 'fd_' + line.split(' ')[1] + ' = cm_->CmOpen(' + line.split(' ')[1] + '_path.c_str() , O_DIRECTORY , ' + line.split(' ')[2] + '); \n\t\t\t\tif ( fd_' + line.split(' ')[1] + ' < 0 ) { \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 6)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 6)        


def insertRemoveFile(contents,option, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( '+ option +'(' + line.split(' ')[1] + '_path.c_str() ) < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


def insertTruncateFile(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( truncate (' + line.split(' ')[1] + '_path.c_str(), ' + line.split(' ')[2] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


def insertClose(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( cm_->CmClose ( fd_' + line.split(' ')[1] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)

def insertRmdir(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( ' + line.split(' ')[0] + '(' + line.split(' ')[1] + '_path.c_str()) < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


def insertFsync(contents, option,  line, index_map, method):
    if option == 'fsync':
        ins = 'cm_->CmFsync'
    elif option == 'fdatasync':
        ins = 'cm_->CmFdatasync'

    to_insert = '\n\t\t\t\tif ( ' + ins + '( fd_' + line.split(' ')[1] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


def insertSync(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tcm_->CmSync(); \n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 2)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 2)


def insertLink(contents, option, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( ' + option + '(' + line.split(' ')[1] + '_path.c_str() , '+ line.split(' ')[2] + '_path.c_str() '+ ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


#def insertCheckpoint(contents, line, index_map, method):
#    
#    to_insert = '\n\t\t\t\tif ( Checkpoint() < 0){ \n\t\t\t\t\treturn -1;\n\t\t\t\t}\n\t\t\t\tlocal_checkpoint += 1; \n\t\t\t\tif (local_checkpoint == checkpoint) { \n\t\t\t\t\treturn 1;\n\t\t\t\t}\n\n'
#    
#    if method == 'setup':
#        contents.insert(index_map['setup'], to_insert)
#        updateSetupMap(index_map, 8)
#    else:
#        contents.insert(index_map['run'], to_insert)
#        updateRunMap(index_map, 8)

def insertCheckpoint(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( cm_->CmCheckpoint() < 0){ \n\t\t\t\t\treturn -1;\n\t\t\t\t}\n\t\t\t\tlocal_checkpoint += 1; \n\t\t\t\tif (local_checkpoint == checkpoint) { \n\t\t\t\t\treturn '+ line.split(' ')[1] + ';\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 8)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 8)


def insertRename(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( cm_->CmRename (' + line.split(' ')[1] + '_path.c_str() , '+ line.split(' ')[2] + '_path.c_str() '+ ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)


def insertFsetxattr(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( ' + line.split(' ')[0] + '( fd_' + line.split(' ')[1] + ', \"user.xattr1\", \"val1 \", 4, 0 ) < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)

def insertRemovexattr(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( ' + line.split(' ')[0] + '(' + line.split(' ')[1] + '_path.c_str() , \"user.xattr1\") < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 4)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 4)

#filename, offset, length
def insertMsync(contents, option, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( cm_->CmMsync ( filep_' + line.split(' ')[1] + ' + ' + line.split(' ')[2] + ', ' + line.split(' ')[3]  + ', MS_SYNC) < 0){\n\t\t\t\t\tcm_->CmMunmap( filep_' + line.split(' ')[1] + ',' + line.split(' ')[2] + ' + ' + line.split(' ')[3] +'); \n\t\t\t\t\treturn -1;\n\t\t\t\t}\n\n'

    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 5)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 5)

def insertMunmap(contents, option, line, index_map, method):
    to_insert = '\n\t\t\t\tcm_->CmMunmap( filep_' + line.split(' ')[1] + ' , 262144);\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
        updateSetupMap(index_map, 2)
    else:
        contents.insert(index_map['run'], to_insert)
        updateRunMap(index_map, 2)



def insertWrite(contents, option, line, index_map, method):
    if option == 'mmapwrite':
        name = 'moffset_' + line.split(' ')[1]
        decl = ' '
        data_decl = ' '
        text_decl = ' '
        filep_decl = ' '
        mmap_decl = ' '
        
        
        if name not in redeclare_map:
            decl = 'int '
            filep_decl = 'char *'
            data_decl = 'void* mdata_' +line.split(' ')[1] + ';'
            text_decl = 'const char *mtext_' + line.split(' ')[1] +'  = \"mmmmmmmmmmklmnopqrstuvwxyz123456\";'
            mmap_decl = filep_decl + 'filep_' + line.split(' ')[1] + ' = (char *) cm_->CmMmap(NULL, 262144, PROT_WRITE|PROT_READ, MAP_SHARED, fd_' + line.split(' ')[1] + ', 0); \t\t\t\tif (filep_' + line.split(' ')[1] + ' == MAP_FAILED) {\t\t\t\t\t return -1;\t\t\t\t}'
            
            redeclare_map[name] = 1
    
        
        to_insert = '\n\t\t\t\tif ( fallocate( fd_' + line.split(' ')[1] + ' , 0 , ' + line.split(' ')[2] + ' , '  + line.split(' ')[3] + ') < 0){ \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1]  +');\n\t\t\t\t\t return errno;\n\t\t\t\t}\n\t\t\t\t' + mmap_decl + '\n\n\t\t\t\t' +decl+ 'moffset_'+ line.split(' ')[1] +' = 0;\n\t\t\t\t' + decl +'m_to_write_'+line.split(' ')[1] +' = ' + line.split(' ')[3] + ' ;\n\t\t\t\t'+ text_decl+ '\n\n\t\t\t\twhile (moffset_'+line.split(' ')[1]+' < '+ line.split(' ')[3] +'){\n\t\t\t\t\tif (m_to_write_'+ line.split(' ')[1] +' < 32){\n\t\t\t\t\t\tmemcpy(filep_'+ line.split(' ')[1]+ ' + ' + line.split(' ')[2] + ' + moffset_'+ line.split(' ')[1] +', mtext_'+ line.split(' ')[1] +', m_to_write_' +line.split(' ')[1]+');\n\t\t\t\t\t\tmoffset_'+ line.split(' ')[1]+' += m_to_write_'+ line.split(' ')[1] +';\n\t\t\t\t\t}\n\t\t\t\t\telse {\n\t\t\t\t\t\tmemcpy(filep_'+ line.split(' ')[1] + ' + ' + line.split(' ')[2] + ' + moffset_' +line.split(' ')[1] + ',mtext_'+line.split(' ')[1] + ', 32);\n\t\t\t\t\t\tmoffset_'+line.split(' ')[1] +' += 32; \n\t\t\t\t\t} \n\t\t\t\t}\n\n'
        
        if method == 'setup':
            contents.insert(index_map['setup'], to_insert)
            updateSetupMap(index_map, 21)
        else:
            contents.insert(index_map['run'], to_insert)
            updateRunMap(index_map, 21)



    elif option == 'write':
        to_insert = '\n\t\t\t\tif ( WriteData ( fd_' + line.split(' ')[1] + ', ' + line.split(' ')[2] + ', ' + line.split(' ')[3] + ') < 0){ \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
        if method == 'setup':
            contents.insert(index_map['setup'], to_insert)
            updateSetupMap(index_map, 5)
        else:
            contents.insert(index_map['run'], to_insert)
            updateRunMap(index_map, 5)      
    else:
        
        name = 'doffset_' + line.split(' ')[1]
        decl = ' '
        data_decl = ' '
        text_decl = ' '
        
        if name not in redeclare_map:
            decl = 'int '
            data_decl = 'void* ddata_' +line.split(' ')[1] + ';'
            text_decl = 'const char *dtext_' + line.split(' ')[1] +'  = \"ddddddddddklmnopqrstuvwxyz123456\";'
            redeclare_map[name] = 1

        # TODO: prevent redeclations here
        to_insert ='\n\t\t\t\tcm_->CmClose(fd_' + line.split(' ')[1] + '); \n\t\t\t\tfd_' + line.split(' ')[1] + ' = cm_->CmOpen(' + line.split(' ')[1] +'_path.c_str() , O_RDWR|O_DIRECT|O_SYNC , 0777); \n\t\t\t\tif ( fd_' + line.split(' ')[1] +' < 0 ) { \n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] +'); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n\t\t\t\t' + data_decl+'\n\t\t\t\tif (posix_memalign(&ddata_' + line.split(' ')[1] +' , 4096, ' + line.split(' ')[3] +' ) < 0) {\n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n\t\t\t\t \n\t\t\t\t' +decl+ 'doffset_'+ line.split(' ')[1] +' = 0;\n\t\t\t\t' + decl +'d_to_write_'+line.split(' ')[1] +' = ' + line.split(' ')[3] + ' ;\n\t\t\t\t'+ text_decl+ '\n\t\t\t\twhile (doffset_'+line.split(' ')[1]+' < '+ line.split(' ')[3] +'){\n\t\t\t\t\tif (d_to_write_'+ line.split(' ')[1] +' < 32){\n\t\t\t\t\t\tmemcpy((char *)ddata_'+ line.split(' ')[1]+ '+ doffset_'+ line.split(' ')[1] +', dtext_'+ line.split(' ')[1] +', d_to_write_' +line.split(' ')[1]+');\n\t\t\t\t\t\tdoffset_'+ line.split(' ')[1]+' += d_to_write_'+ line.split(' ')[1] +';\n\t\t\t\t\t}\n\t\t\t\t\telse {\n\t\t\t\t\t\tmemcpy((char *)ddata_'+ line.split(' ')[1] +'+ doffset_'+line.split(' ')[1] +',dtext_'+line.split(' ')[1] +', 32);\n\t\t\t\t\t\tdoffset_'+line.split(' ')[1] +' += 32; \n\t\t\t\t\t} \n\t\t\t\t} \n\n\t\t\t\tif ( pwrite ( fd_' + line.split(' ')[1] + ', ddata_'+ line.split(' ')[1] + ', '  + line.split(' ')[3] + ', ' + line.split(' ')[2] +') < 0){\n\t\t\t\t\tcm_->CmClose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\t\t\t\tcm_->CmClose(fd_' + line.split(' ')[1] + ');\n\n'

        if method == 'setup':
            contents.insert(index_map['setup'], to_insert)
            updateSetupMap(index_map, 32)
        else:
            contents.insert(index_map['run'], to_insert)
            updateRunMap(index_map, 32)


# Insert a function in 'line' into 'file' at location specified by 'index_map' in the specified 'method'
# If the workload has functions with various possible paramter options, the 'permutation' defines the set of
# paramters to be set in this file.

def insertFunctions(line, file, index_map, method):
    with open(file, 'r+') as insert:
        
        contents = insert.readlines()


        if line.split(' ')[0] == 'falloc':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)

            insertFalloc(contents, line, index_map, method)
            if line.split(' ')[-2] == 'addToSetup':
                line = line.replace(line.split(' ')[1], line.split(' ')[-1], 1)
                insertFalloc(contents, line, index_map, 'setup')
    
        elif line.split(' ')[0] == 'mkdir':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertMkdir(contents, line, index_map, method)

        elif line.split(' ')[0] == 'mknod':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertMknodFile(contents, line, index_map, method)


        elif line.split(' ')[0] == 'open':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertOpenFile(contents, line, index_map, method)

        elif line.split(' ')[0] == 'opendir':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertOpenDir(contents, line, index_map, method)            
                
        elif line.split(' ')[0] == 'remove' or line.split(' ')[0] == 'unlink':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertRemoveFile(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'close':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertClose(contents, line, index_map, method)

        elif line.split(' ')[0] == 'rmdir':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertRmdir(contents, line, index_map, method)

        elif line.split(' ')[0] == 'truncate':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertTruncateFile(contents, line, index_map, method)

        elif line.split(' ')[0] == 'fsync' or line.split(' ')[0] == 'fdatasync':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertFsync(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'sync':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertSync(contents, line, index_map, method)
        
        elif line.split(' ')[0] == 'checkpoint':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertCheckpoint(contents, line, index_map, method)

        elif line.split(' ')[0] == 'rename':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertRename(contents, line, index_map, method)

        elif line.split(' ')[0] == 'fsetxattr':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertFsetxattr(contents, line, index_map, method)

        elif line.split(' ')[0] == 'removexattr':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertRemovexattr(contents, line, index_map, method)

        elif line.split(' ')[0] == 'link' or line.split(' ')[0] == 'symlink':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertLink(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'write' or line.split(' ')[0] == 'dwrite' or line.split(' ')[0] == 'mmapwrite':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertWrite(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'msync':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertMsync(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'munmap':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            option = line.split(' ')[0]
            insertMunmap(contents, option, line, index_map, method)

        elif line.split(' ')[0] == 'none':
            pass
#            if method == 'setup':
#                updateSetupMap(index_map, 1)
#            else:
#                updateRunMap(index_map, 1)
#            #we know this can be only in run
#            contents.insert(index_map['run'], '\t\t\t\t return 1;\n')
#            updateRunMap(index_map, -1)

        insert.seek(0)
        insert.writelines(contents)
        insert.close()
            



#def hasVariation(keyword):
#    if keyword in ['fsync', 'fdatasync', 'falloc', 'remove', 'unlink', 'link', 'symlink', 'write']:
#        return True
#
#def buildTuple(command):
#    if command == 'fsync' or command == 'fdatasync' :
#        d = tuple(FsyncOptions)
#    elif command == 'falloc':
#        d = tuple(FallocOptions)
#    elif command == 'remove' or command == 'unlink':
#        d = tuple(RemoveOptions)
#    elif command == 'link' or command == 'symlink':
#        d = tuple(LinkOptions)
#    elif command == 'write':
#        d = tuple(WriteOptions)
#    else:
#        d=()
#    return d



def main():
    
    #open log file
#    log_file = time.strftime('%Y%m%d_%H%M%S') + '-workloadGen.log'
#    log_file_handle = open(log_file, 'w')

    #Parse input args
    parsed_args = build_parser().parse_args()

    #Print the test setup - just for sanity
    # base_test = 'code/tests/base_test.cpp'
#    print_setup(parsed_args)

    
    #check if test file exists
    if not os.path.exists(parsed_args.test_file) or not os.path.isfile(parsed_args.test_file):
        print parsed_args.test_file + ' : No such test file\n'
        exit(1)
    
    #Create the target directory
    create_dir(parsed_args.target_path)

    #Create a pre-populated dictionary of replacable operations
    operation_map = create_dict()

    #Copy base file to target path
    base_test = parsed_args.base_file
    base_file = parsed_args.target_path + "/" + base_test.split('/')[-1]
    # copyfile(base_test, base_file)
    test_file = parsed_args.test_file

    index_map = {'define' : 0, 'setup' : 0, 'run' : 0, 'check' : 0}
    
    #iterate through the base file and populate these values
    index = 0
    with open(base_file, 'r') as f:
        contents = f.readlines()
        for index, line in enumerate(contents):
            index += 1
            line = line.strip()
            if line.find('setup') != -1:
                if line.split(' ')[2] == 'setup()':
                    index_map['setup'] = index
            elif line.find('run') != -1:
                if line.split(' ')[2] == 'run(':
                    index_map['run'] = index
            elif line.find('check_test') != -1:
                if line.split(' ')[2] == 'check_test(':
                    index_map['check'] = index
            elif line.find('private') != -1:
                if line.split(' ')[0] == 'private:':
                    index_map['define'] = index
    f.close()


#    #Iterate through the lang spec and identify all possible lines that have alternative parameter options
#    done_list = []
#    with open(test_file, 'r') as f:
#        for line in f:
#        
#            #ignore newlines
#            if line.split(' ')[0] == '\n':
#                continue
#            
#            #Remove leading, trailing spaces
#            line = line.strip()
#            
#            #if the line starts with #, it indicates which region of base file to populate and skip this line
#            if line.split(' ')[0] == '#' :
#                method = line.strip().split()[-1]
#                continue
#
#            if hasVariation(line.split(' ')[0] ) is True:
#                d = buildTuple(line.split(' ')[0])
#                done_list.append(d)
#
#
#    f.close()
#
#    count = 0
#    permutations = []
#    for i in itertools.product(*done_list):
#        permutations.append(i)
#        count +=1
#
#    print 'Total files being created = ', count


#    #Now populate count num of files
#    val = 0
#    for permutation in permutations:

    val = 0
    new_file = test_file  + ".cpp"
    new_file = parsed_args.target_path + new_file
    copyfile(base_file, new_file)

    new_index_map = index_map.copy()
#        log = ' ,'.join(permutation);
#        log = `val` + ' : ' + log + '\n'
#        log_file_handle.write(log)
        #Iterate through test file and fill up method by method
    with open(test_file, 'r') as f:
        iter = 0
        for line in f:

            #ignore newlines
            if line.split(' ')[0] == '\n':
                continue
            
            #Remove leading, trailing spaces
            line = line.strip()
            
            #if the line starts with #, it indicates which region of base file to populate and skip this line
            if line.split(' ')[0] == '#' :
                method = line.strip().split()[-1]
                continue
            
            if method == 'define':
                insertDefine(line, new_file, new_index_map)
            
            elif method == 'declare':
                insertDeclare(line, new_file, new_index_map)

            elif (method == 'setup' or method == 'run'):
                op_map={}
                insertFunctions(line, new_file, new_index_map, method)

    f.close()
    val += 1


#    log_file_handle.close()


if __name__ == '__main__':
	main()
