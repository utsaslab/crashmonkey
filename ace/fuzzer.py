#!/usr/bin/env python3

# Usage: fuzzer.py [-h] --test_file TEST_FILE --output_dir OUTPUT_DIR
#
# Workload Fuzzer for CrashMonkey v1.0

# Optional arguments:
#   -h, --help            show this help message and exit
#   --test_file TEST_FILE, -t TEST_FILE
#                         J-lang test file to use as seed for fuzzer
#   --output_dir OUTPUT_DIR, -o OUTPUT_DIR
#                         Directory to save the generated test files

import os
import sys
import ntpath
import argparse
import itertools
import subprocess
from shutil import copyfile

import common

def log(msg, flush=False):
    sys.stdout.write(str(msg))
    if flush: sys.stdout.flush()

def parse_jlang(filename):
    run_starts = False

    ignore = ["none"]
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
        r = parts[:]
        r[0] = "symlink"
        perms.append(" ".join(r))
    elif parts[0] == "symlink":
        r = parts[:]
        r[0] = "link"
        perms.append(" ".join(r))
    elif parts[0] == "remove" or parts[0] == "unlink":
        pass
    elif parts[0] == "fsetxattr":
        pass
    elif parts[0] == "removexattr":
        pass
    elif parts[0] == "truncate":
        pass
    elif parts[0] == "dwrite":
        r = parts[:]
        for replace in ['write', 'mmapwrite']:
            r[0] = replace
            perms.append(" ".join(r))
    elif parts[0] == "write":
        r = parts[:]
        for replace in ['dwrite', 'mmapwrite']:
            r[0] = replace
            perms.append(" ".join(r))
    elif parts[0] == "mmapwrite":
        r = parts[:]
        for replace in ['dwrite', 'write']:
            r[0] = replace
            perms.append(" ".join(r))
    elif parts[0] == "fsync":
        perms.append("sync")

        perms.append("fdatasync " + parts[1])
    elif parts[0] == "fdatasync":
        perms.append("sync")
        perms.append("fsync " + parts[1])
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
    elif parts[0] == "checkpoint" or parts[0] == "close":
        pass
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
    test_name = ntpath.basename(test_file)
    base_jlang = os.path.join(output_dir, "base-j-lang")
    base_cm = os.path.join(output_dir, "base.cpp")

    if not output_dir.endswith("/"):
        output_dir += "/"

    # Function to generate numbered filenames of the format:
    #     <test_file>-fuzz<num>
    num_created = 1
    def make_name():
        nonlocal num_created

        name = "{}-fuzz{}".format(test_name, num_created)
        num_created += 1
        return name

    # Single-operation mutation. For each operation, try all
    # possible modifications.
    options = [get_permutations(i) for i in instructions]
    modified_instructions = instructions[:]
    for i, instr in enumerate(instructions):
        for option_i in options[i]:
            modified_instructions[i] = option_i

            # Write the list of modified instructions to a j-lang file
            write_jlang_file(modified_instructions, make_name(), base_jlang)

        modified_instructions[i] = instr

    # Convert the original J-lang file using the Crashmonkey Adapter.
    cmd = "python3 cmAdapter.py -b {} -t {} -p {}\n".format(base_cm, test_file, output_dir)
    subprocess.call(cmd, shell=True)
    copyfile(test_file, os.path.join(output_dir, test_name))

    # Convert the generated J-lang files.
    for i in range(1, num_created):
        jlang_name = "{}-fuzz{}".format(test_name, i)

        cmd = "python3 cmAdapter.py -b {} -t {} -p {}\n".format(
                base_cm,
                jlang_name,
                output_dir)
        subprocess.call(cmd, shell=True)

        # Move J-lang file to output folder.
        os.rename(jlang_name, os.path.join(output_dir, jlang_name))


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

    # Copy base Crashmonkey file to outpt directory
    base_cm = os.path.join(cwd, "../code/tests/ace-base/base.cpp")
    copyfile(base_cm, os.path.join(parsed_args.output_dir, "base.cpp"))

    instructions = parse_jlang(parsed_args.test_file)
    fuzz(instructions, parsed_args.test_file, parsed_args.output_dir)


if __name__ == '__main__':
    main()
