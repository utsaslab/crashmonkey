#!/usr/bin/env python

# TODO: put some usage instructions here

import os
import sys
import argparse
import itertools
import ntpath
from shutil import copyfile

import common

def log(msg, flush=False):
    sys.stdout.write(msg)
    if flush: sys.stdout.flush()

def parse_jlang(filename):
    run_starts = False

    ignore = ["checkpoint", "close", "none"]
    instructions = []

    for line in open(filename, "r"):
        if run_starts:
            if not any(line.startswith(i) for i in ignore):
                instructions.append(line.strip())
        elif "# run" in line:
            run_starts = True

    return instructions

def get_permutations(instruction):
    parts = instruction.split()
    perms = []

    if parts[0] == "open":
        pass
    elif parts[0] == "opendir" or parts[0] == "mkdir":
        pass
    elif parts[0] == "link":
        pass
    elif parts[0] == "symlink":
        pass
    elif parts[0] == "remove" or parts[0] == "unlink":
        pass
    elif parts[0] == "fsetxattr":
        pass
    elif parts[0] == "removexattr":
        pass
    elif parts[0] == "truncate":
        pass
    elif parts[0] == "dwrite":
        pass
    elif parts[0] == "write":
        pass
    elif parts[0] == "mmapwrite":
        pass
    elif parts[0] == "fsync":
        pass
    elif parts[0] == "fdatasync":
        pass
    elif parts[0] == "sync":
        pass
    elif parts[0] == "rename":
        pass
    elif parts[0] == "rmdir":
        pass
    elif parts[0] == "falloc":
        filename, mode, offset, length = parts[1:]

        for mode_i in common.FallocOptions:
            if mode != mode_i:
                perms.append("falloc {} {} {} {}".format(filename, mode_i, offset, length))
    else:
        print("Unknown instruction: {}".format(instruction))
        sys.exit(1)

    return perms

def write_jlang_file(lines, filename, base_jlang):
    copyfile(base_jlang, filename)

    log("Writing to file {}\n".format(filename))
    with open(filename, "a") as f:
        f.write("\n# run\n")

        add_newline = lambda f: f + "\n"
        lines = list(map(add_newline, lines))
        f.writelines(lines)

def fuzz(instructions, test_file, output_dir):
    basename = ntpath.basename(test_file)
    base_jlang = os.path.join(output_dir, "base-j-lang")
    num_created = 0

    def make_name():
        nonlocal num_created

        num_created += 1
        base = "{}-fuzz{}".format(basename, num_created)
        return os.path.join(output_dir, base)

    options = [get_permutations(i) for i in instructions]

    modified_instructions = instructions[:]
    for i, instr in enumerate(instructions):
        for option_i in options[i]:
            modified_instructions[i] = option_i

            write_jlang_file(modified_instructions, make_name(), base_jlang)

        modified_instructions[i] = instr

def build_parser():
    parser = argparse.ArgumentParser(description='Workload Fuzzer for CrashMonkey v1.0')

    parser.add_argument('--test_file', '-t', 
            required=True, help='J-lang test file to use as seed for fuzzer')
    parser.add_argument('--output_dir', '-o', 
            required=True, help='Directory to save the generated test files')

    return parser

def main():
    parsed_args = build_parser().parse_args()

    # Ensure J-lang file exists
    if not os.path.isfile(parsed_args.test_file):
        print("{} : File not found".format(parsed_args.test_file))
        sys.exit(1)

    # Create output directory if it does not exist
    if not os.path.isdir(parsed_args.output_dir):
        if os.path.exists(parsed_args.output_dir):
            print("{} already exists but is not directory".format(parsed_args.output_dir))
            sys.exit(1)
        os.makedirs(parsed_args.output_dir)

    # Copy base J-lang file to output directory
    cwd = os.path.dirname(os.path.realpath(__file__))
    base_j_lang = os.path.join(cwd, "../code/tests/ace-base/base-j-lang")
    copyfile(base_j_lang, os.path.join(parsed_args.output_dir, "base-j-lang"))

    instructions = parse_jlang(parsed_args.test_file)
    fuzz(instructions, parsed_args.test_file, parsed_args.output_dir)


if __name__ == '__main__':
    main()
