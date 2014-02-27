# 
# This script replaces the minisat sources with the ones from orig-dist/minisat, 
# and modifies the new sources so that they compile and link in BOLT.
#
# Run as python import-orig-dist.py from extsrc/minisat-gh directory.
#
# Assumes that subdirectory orig-dist contains the clone of minisat repo from github.
#
# (c) 2013, Anton Belov
#

from __future__ import print_function
import os
import glob
import shutil
import fileinput
import sys

# substitutions map
smap = {
    'and' : 'anD',
    'xor' : 'xoR',
    'new' : 'neW',
    '#include "lglib.h"' : '#include "lglib.hh"',
    'extern char ** environ;' : '//extern char ** environ;'
}

# original source root
orig_root = 'orig-dist/'

# 1. bring new copies
print("Cleaning files ...")
for f in glob.glob('lglib.*'): os.remove(f)

# 2. copy the new files
print("Bringing new files ...")
shutil.copy(orig_root + 'lglib.h', 'lglib.hh') 
shutil.copy(orig_root + 'lglib.c', 'lglib.cc') 

# 3. modify lglib.hh
print("Modifying lglib.hh")
for line in fileinput.input('lglib.hh', inplace=True):
    if line.startswith('#endif'):
        print('}')
    print(line, end='')
    if line.startswith('#define LGL_UNSATISFIABLE'):
        print('\nnamespace LingelingALA {')

# 3. modify lglib.cc
print("Modifying lglib.cc")
extra_code = '''
#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char ** environ;
#endif

namespace LingelingALA {
'''
first_endif = True
for line in fileinput.input('lglib.cc', inplace=True):
    for (s1,s2) in smap.iteritems():
        line = line.replace(s1, s2)
    print(line, end='')
    if first_endif and line.startswith('#endif'):
        print(extra_code)
        first_endif = False
os.system('echo "}" >> lglib.cc')
