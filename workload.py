#!/usr/bin/env python

#To run : python workload.py -t code/tests/test -p code/tests/target_dir
import os
import re
import sys
import stat
import subprocess
import argparse
import time
from shutil import copyfile
from string import maketrans

def enum(**enums):
    return type('Enum', (), enums)

#Not using this currently. We need to add a way to track the parameter combinations already used and return a unused value each time
FallocOptions = enum(ONE='FALLOC_FL_ZERO_RANGE', TWO='FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE', THREE='FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE', FOUR='0', FIVE = 'FALLOC_FL_KEEP_SIZE')

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

class Command(object):
    variations = []
    def __init__(self, line, index, operation):
        self.line = line
        self.index = index
        self.op = operation
        self.variations = self.op.variants

class Variations(object):
    op = ""
    
    def __init__(self):
        self.variants = []


class FallocVariations(Variations):
    #Here we will skip collapse_range for now: btrfs doesn't support it anyway
    # Punch hole must always be ored with keep size
    # Normal falloc (0) and with -k
    # zero range and with -k
    op = "fallocate"
    fd = 0
    mode = '0'
    index = 0
    length = 0
    
    def __init__(self):
        self.variants = ['0', 'FALLOC_FL_KEEP_SIZE',  'FALLOC_FL_ZERO_RANGE', 'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE', 'FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE']

    def Check_Test_Change(self, option, file):
        self.variants
#        if(option == 0):
#            #Do nothing. Check if original file = state after falloc
#        elif(option == FALLOC_KEEP_SIZE):
#            #Check file size remains same
#            #Check block count increase if beyond eof


    def Replace(self, text, variant_name, line, map):
        #int fallocate(int fd, int mode, off_t index, off_t len);
        #let's split parameters by ,
        line_strip = line.split('(')[-1].split(')')[0]
        fd = re.sub(r"[\\n\\t\s]*", "", line_strip.split(',')[0])
        mode = re.sub(r"[\\n\\t\s]*", "", line_strip.split(',')[1])
        index = re.sub(r"[\\n\\t\s]*", "", line_strip.split(',')[2])
        length = re.sub(r"[\\n\\t\s]*", "", line_strip.split(',')[3])
        print 'Mode : ' + mode
        if mode in self.variants and not mode in map:
            map[mode] = True
        else:
            map[variant_name] = True
            text = text.replace()
                    
        
        #we can return map of done - whatever is completed
        return text

class OpenVariations(Variations):
    #Here we will skip collapse_range for now: btrfs doesn't support it anyway
    # Punch hole must always be ored with keep size
    # Normal falloc (0) and with -k
    # zero range and with -k
    op = "open"
    
    def __init__(self):
        self.variants = ['O_RDWR | O_CREAT', 'O_DIRECT', 'mmap' ]
    
    def Check_Test_Change(self, option, file):
        self.variants

    def Replace(self, text, variant_name, line, map):
        print line
        return text

class RemoveVariations(Variations):
    #Here we will skip collapse_range for now: btrfs doesn't support it anyway
    # Punch hole must always be ored with keep size
    # Normal falloc (0) and with -k
    # zero range and with -k
    op = "remove"
    
    def __init__(self):
        self.variants = ['remove', 'unlink']
    
    def Check_Test_Change(self, option, file):
        self.variants
    
    def Replace(self, text, variant_name, line, map):
        print line
        return text

class FsyncVariations(Variations):
    #Here we will skip collapse_range for now: btrfs doesn't support it anyway
    # Punch hole must always be ored with keep size
    # Normal falloc (0) and with -k
    # zero range and with -k
    op = "fsync"
    
    def __init__(self):
        self.variants = ['fsync', 'fdatasync']
    
    def Check_Test_Change(self, option, file):
        self.variants
    
    def Replace(self, text, variant_name, line,  map):
        print line
        return text

def build_parser():
    parser = argparse.ArgumentParser(description='Workload Generator for XFSMonkey v1.0')

    # global args
    parser.add_argument('--test_file', '-t', default='', help='Test file to generate workload')

    # crash monkey args
    parser.add_argument('--target_path', '-p', default='code/tests/', help='Directory to save the generated test files')

    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Base test file', parsed_args.test_file)
    print '{0:20}  {1}'.format('Target directory', parsed_args.target_path)
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

def insertDefine(line, file, index_map):
    with open(file, 'r+') as define:
        
        contents = define.readlines()
        
        #Initialize paths in setup phase
        updateSetupMap(index_map, 1)
        to_insert = '\t\t\t\t' + line.split('/')[-1] + '_path = mnt_dir_' + ' + "/' + line + '";\n'
        contents.insert(index_map['setup'], to_insert)
        
        #Initialize paths in run phase
        updateRunMap(index_map, 1)
        to_insert = '\t\t\t\t' + line.split('/')[-1] + '_path =  mnt_dir_/' + ' + "/' + line + '";\n'
        contents.insert(index_map['run'], to_insert)
        
        #Initialize paths in check phase
        updateCheckMap(index_map, 1)
        to_insert = '\t\t\t\t' + line.split('/')[-1] + '_path =  mnt_dir_/' + ' + "/' + line + '";\n'
        contents.insert(index_map['check'], to_insert)
        
        #Update defines portion
        #Get only the file name. We don't want the path here
        updateDefineMap(index_map, 1)
        to_insert = '\t\t\t string ' + line.split('/')[-1] + '_path; \n'
        contents.insert(index_map['define'], to_insert)
        
        define.seek(0)
        define.writelines(contents)
        define.close()

#def getFallocVariant(line_num, op_map):

def insertFalloc(contents, option, line, index_map, method):

    to_insert = '\n\t\t\t\tif ( fallocate( fd_' + line.split(' ')[1] + ' , ' + option + ' , ' + line.split(' ')[2] + ' , '  + line.split(' ')[3] + ') < 0){ \n\t\t\t\t\t close(' + line.split(' ')[1]  +');\n\t\t\t\t\t return errno;\n\t\t\t\t}\n\n'

    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)

    if method == 'setup':
        updateSetupMap(index_map, 5)
    elif method == 'run':
        updateRunMap(index_map, 5)


def insertMkdir(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( mkdir(' + line.split(' ')[1] + '_path.c_str() , ' + line.split(' ')[2] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 4)
    elif method == 'run':
        updateRunMap(index_map, 4)


def insertOpenFile(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tconst int fd_' + line.split(' ')[1] + ' = open(' + line.split(' ')[1] + '_path.c_str() , ' + line.split(' ')[2] + ' , ' + line.split(' ')[3] + '); \n\t\t\t\tif ( fd_' + line.split(' ')[1] + ' < 0 ) { \n\t\t\t\t\tclose( fd_' + line.split(' ')[1] + '); \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 6)
    elif method == 'run':
        updateRunMap(index_map, 6)


def insertRemoveFile(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( '+ line.split(' ')[0] +'(' + line.split(' ')[1] + '_path.c_str() ) < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 4)
    elif method == 'run':
        updateRunMap(index_map, 4)

def insertClose(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( ' + line.split(' ')[0] + '( fd_' + line.split(' ')[1] + ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 4)
    elif method == 'run':
        updateRunMap(index_map, 4)

def insertCheckpoint(contents, line, index_map, method):
    
    to_insert = '\n\t\t\t\tif ( Checkpoint() < 0){ \n\t\t\t\t\treturn -1;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 4)
    elif method == 'run':
        updateRunMap(index_map, 4)

def insertRename(contents, line, index_map, method):
    to_insert = '\n\t\t\t\tif ( ' + line.split(' ')[0] + '(' + line.split(' ')[1] + '_path.c_str() , '+ line.split(' ')[2] + '_path.c_str() '+ ') < 0){ \n\t\t\t\t\treturn errno;\n\t\t\t\t}\n\n'
    
    if method == 'setup':
        contents.insert(index_map['setup'], to_insert)
    else:
        contents.insert(index_map['run'], to_insert)
    
    if method == 'setup':
        updateSetupMap(index_map, 4)
    elif method == 'run':
        updateRunMap(index_map, 4)


def insertFunctions(line, file, index_map, method, op_map):
    with open(file, 'r+') as insert:
        #with each line number of input test file, associate a map of possible ops. Get the next possible options for the op here

        contents = insert.readlines()
        print line
        if line.split(' ')[0] == 'falloc':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map
                             , 1)
#            option = getFallocVariant(line_num, op_map)
            option = FallocOptions.ONE
            insertFalloc(contents, option, line, index_map, method)
    
        elif line.split(' ')[0] == 'mkdir':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertMkdir(contents, line, index_map, method)

        elif line.split(' ')[0] == 'open':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertOpenFile(contents, line, index_map, method)
                
        elif line.split(' ')[0] == 'remove' or line.split(' ')[0] == 'unlink':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertRemoveFile(contents, line, index_map, method)

        elif line.split(' ')[0] == 'close' or line.split(' ')[0] == 'fsync' or line.split(' ')[0] == 'fdatasync':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertClose(contents, line, index_map, method)
                
        elif line.split(' ')[0] == 'checkpoint':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertCheckpoint(contents, line, index_map, method)

        elif line.split(' ')[0] == 'rename' or line.split(' ')[0] == 'link' or line.split(' ')[0] == 'symlink':
            if method == 'setup':
                updateSetupMap(index_map, 1)
            else:
                updateRunMap(index_map, 1)
            insertRename(contents, line, index_map, method)

        insert.seek(0)
        insert.writelines(contents)
        insert.close()

        #if method is setup, update setup index, else run index





def main():

    # Open the log file
#    log_file = time.strftime('%Y%m%d_%H%M%S') + '-workloadGen.log'
#    log_file_handle = open(log_file, 'w')
#    original = sys.stdout
#    sys.stdout = Log(sys.stdout, log_file_handle)

    #Parse input args
    parsed_args = build_parser().parse_args()

    #Print the test setup - just for sanity
    base_test = 'code/tests/base_test.cpp'
    print_setup(parsed_args)
    
    
    #check if test file exists
    if not os.path.exists(parsed_args.test_file) or not os.path.isfile(parsed_args.test_file):
        print parsed_args.test_file + ' : No such test file\n'
        exit(1)
    
    #Create the target directory
    create_dir(parsed_args.target_path)

    #Create a pre-populated dictionary of replacable operations
    operation_map = create_dict()

    #Copy base file to target path
    base_file = parsed_args.target_path + "/" + base_test.split('/')[-1]
    copyfile(base_test, base_file)
    test_file = parsed_args.test_file

    val = 0
    new_file = base_file.split('.cpp', 1)[0] + "_" + str(val) + ".cpp"
    copyfile(base_file, new_file)

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
                if line.split(' ')[2] == 'run()':
                    index_map['run'] = index
            elif line.find('check') != -1:
                if line.split(' ')[2] == 'check_test(':
                    index_map['check'] = index
            elif line.find('private') != -1:
                if line.split(' ')[0] == 'private:':
                    index_map['define'] = index
    print index_map
    f.close()
    
    #Iterate through test file and fill up base_file portion by portion
    with open(test_file, 'r') as f:
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
                insertDefine(line, new_file, index_map)
                print index_map

            elif (method == 'setup' or method == 'run'):
                op_map={}
                insertFunctions(line, new_file, index_map, method, op_map)
                print index_map





    f.close()
#    #iterate through the base file and update operation map
#    found = False
#    line_list = []
#    line_index = []
#    command_list = []
#    index = 0
#    newline = ""
#    with open(base_file, 'r') as f:
#        for line in f:
#            for i in line.strip().split():
#                key = i.split('(', 1)[0]
#                value = operation_map.get(key)
#                if value is not None:
#                    value += 1
#                    found = True
#                    line_index.append(index)
#                    true_key = key
#                    operation_map[key] = value
#            if found == True and not i.endswith(';') and not i.endswith('{'):
#                newline = newline + line
#                while not i.endswith(';') or not i.endswith('{'):
#                    break
#            elif found == True:
#                newline = newline + line
#                line_list.append(newline)
#                if (true_key == 'open'):
#                    c = Command(newline, line_index[-1], OpenVariations())
#                elif (true_key == 'fsync'):
#                    c = Command(newline, line_index[-1], FsyncVariations())
#                elif (true_key == 'remove'):
#                    c = Command(newline, line_index[-1], RemoveVariations())
#                elif (true_key == 'fallocate'):
#                    c = Command(newline, line_index[-1], FallocVariations())
#                command_list.append(c)
##                print "\nLine = ", c.line
##                print "index = ", c.index
##                print "Operation = ", c.op.op
##                print "Variants = ", c.op.variants
#                newline = ""
#                found = False
#
#            index += len(line)
#
#        print line_index
#
##        f.seek(line_index[0])
##        print f.readline()
##        f.seek(line_index[1])
##        print f.readline()
##        f.seek(line_index[2])
##        print f.readline()
##        f.seek(line_index[3])
##        print f.readline()
#
#
#
##    #Find max possible permutation
##    perm = 0
##    val = 1
##    for i in operation_map:
##        if(i =='open'):
##            val*= 2*operation_map[i]
##        elif(i =='fallocate'):
##            val*= 3*operation_map[i]
##        elif(i == 'fsync'):
##            val*= 2*operation_map[i]
##        else:
##            val*=1
##    print val
#
#    val = 0
##    done_map = [dict() for x in range(len(command_list))]
#    done_map = []
#    for command in command_list:
#        d = dict()
#        done_map.append(d.copy())
#        for com in command.op.variants:
#            print com
#            new_file = base_file.split('.cpp', 1)[0] + "_" + str(val) + ".cpp"
#            copyfile(base_file, new_file)
#            
#            #replace by one variant
#            f = open(new_file, "r+")
#            content = f.read()
#            content = command.op.Replace(content, com, command.line, done_map[val])
##            content = content.replace(command.line, "Change made\n")
#            f.write(content)
#            f.close()
#        val +=1
##    print content
#
##    sys.stdout = original
##    log_file_handle.close()
#
#
#    #Now for all possible changes : while changes is true :
#        #Create a test file
#        #Make the change
#        #add corresponding extra check_test if any


if __name__ == '__main__':
	main()
