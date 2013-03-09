#!/usr/bin/env python

import os
import sys
import string
from datetime import datetime

source_exts = ('.cpp', '.c', '.cc')
header_exts = ('.hpp', '.h')
sln_exts = ('.sln')
proj_exts = ('.vcproj')

CREATE_PRI_FILES = False

ignore_dirs = ('.svn', 'GeneratedFiles', 'generatedfiles',
               '_UpgradeReport_Files', 'Debug', 'Release',
               'Logs', 'debug', 'release')
               
pro_template = string.Template('''# $basename.pro

# Auto-generated by generate-pro.py
# Do not edit this file.

# Put additional options in the corresponding $basename.pri

PROJECT = $project
$template
CONFIG *= debug_and_release
CONFIG *= build_all
CONFIG *= warn_on
CONFIG *= console

PROJECT_PATH = $proj_path
PROJECT_LIBRARY_PATH = $proj_path/Libraries
PAVE_COMMON_PATH = $pave_common
PAVE_COMMON_LIBRARY_PATH = $pave_common/Libraries

$body
# optional pri file to specify additional build options and dependencies
include( $basename.pri )

# output dir for build outputs
CONFIG(debug, debug|release) {
    DESTDIR = $$$${PROJECT_PATH}/debug
    TARGET = $target_debug
}
CONFIG(release, debug|release) {
    DESTDIR = $$$${PROJECT_PATH}/release
    TARGET = $target
}

QMAKE_LIBPATH *= $$$${DESTDIR}
include( $$$${PROJECT_LIBRARY_PATH}/Libraries.pri )
include( $$$${PAVE_COMMON_LIBRARY_PATH}/Libraries.pri )
include( $$$${PAVE_COMMON_PATH}/Common.pri )
''')

start_dir = os.path.abspath(sys.argv[1])  # subdirectory of either IGVC or P12
project_root = os.path.abspath('.')  # current directory, either IGVC or P12
pave_common_root = os.path.normpath(os.path.join(project_root, '../PAVE_Common'))

project_root_basename = os.path.basename(project_root)
if project_root_basename != 'IGVC' and project_root_basename != 'P12':
    print 'Current directory must be project root.'
    exit()

# build directory structure of the form:
class Dir:
    def __init__(self, root_name, dirs, source_files, header_files, proj_descendents, sln_descendents):
        self.root_name = root_name
        self.dirs = dirs
        self.source_files = source_files
        self.header_files = header_files
        self.proj_descendents = proj_descendents
        self.sln_descendents = sln_descendents

# check if file has one of the extensions in type_exts
def is_type(file, type_exts):
    return os.path.splitext(file)[1] in type_exts

# returns a list of the elements of files that have extensions in type_exts
def get_files_of_type(files, type_exts):
    return [file for file in files if is_type(file, type_exts)]
    
# return the argument, quoted if it has spaces
def quote_name(name):
    if ' ' in name:
        return '"' + name + '"'
    else:
        return name
        
# return the include string for .pri solution subdirectories, such as
#   include( Vision/Vision.pri )
def subdir_sln_name(sln_dir):
    file_name = os.path.basename(sln_dir) + '.pri'
    return 'include( %s )' % quote_name(os.path.join(sln_dir, file_name))
       
# replaces backslashes with forward, to avoid qmake warnings
def qmake_path(s):
    return s.replace('\\', '/')
    
# creates the text of the .pro file for the directory root (of type Dir)
# returns a tuple of (basename of generated .pro file, text of file)
def generate_pro_text(root):
    source_files = root.source_files
    header_files = root.header_files
    
    source_list = []
    header_list = []
    subdir_list = []
    subdir_pri_list = []
    template = ''
    lib_config = ''
    
    proj_path = qmake_path(os.path.relpath(project_root, root.root_name))
    pave_common = qmake_path(os.path.relpath(pave_common_root, root.root_name))
        
    project_name = os.path.basename(os.path.abspath(root.root_name))
    pro_pri_basename = project_name
    
    d = dict(project = project_name,
             proj_path = proj_path,
             pave_common = pave_common,
             basename = pro_pri_basename,
             target = project_name,
             target_debug = project_name,
             template = '')

    if len(root.proj_descendents) > 0:  # this is a solution directory
        d['template'] = 'TEMPLATE = subdirs\n'
        subdir_list = ['SUBDIRS *= %s' % quote_name(proj_dir)
            for proj_dir in root.proj_descendents]
        subdir_pri_list = [subdir_sln_name(sln_dir)
            for sln_dir in root.sln_descendents]
    elif len(source_files) > 0:  # this is a project directory
        source_list = ['SOURCES *= ' + quote_name(file) for file in source_files]
        header_list = ['HEADERS *= ' + quote_name(file) for file in header_files]
    else:
        # print 'Doing nothing in ' + root.root_name
        return ('', '')

    body_list = []
    if len(source_list) > 0:
        body_list.append("\n".join(source_list) + '\n')
    if len(header_list) > 0:
        body_list.append("\n".join(header_list) + '\n')
    if len(subdir_list) > 0:
        body_list.append("\n".join(subdir_list) + '\n')
    if len(subdir_pri_list) > 0:
        body_list.append("\n".join(subdir_pri_list) + '\n')
    
    d['body'] = qmake_path("\n".join(body_list))

    return (pro_pri_basename, pro_template.substitute(d))
    
# prunes out directories with names in ignore_dirs
def pruned_dirs(dirs):
    return [dir for dir in dirs if os.path.basename(dir) not in ignore_dirs]
    
# returns a clean, readable path for directory child, a subdirectory of parent_dir
def child_item(parent_dir, child):
    return os.path.normpath(os.path.join(parent_dir, child))
    
def map_files(start_dir):
    items = os.listdir(start_dir)
    files = [item for item in items if os.path.isfile(os.path.join(start_dir, item))]
    dirs = [item for item in items if os.path.isdir(os.path.join(start_dir, item))]
    dirs = pruned_dirs(dirs)
    sub_fdirs = [map_files(child_item(start_dir, dir)) for dir in dirs]
    
    source_files = get_files_of_type(files, source_exts)
    header_files = get_files_of_type(files, header_exts)
    proj_descendents = [child_item(os.path.basename(sub_fdir.root_name), proj_dir)
        for sub_fdir in sub_fdirs for proj_dir in sub_fdir.proj_descendents]
    sln_descendents = [child_item(os.path.basename(sub_fdir.root_name), sln_dir)
        for sub_fdir in sub_fdirs for sln_dir in sub_fdir.sln_descendents]
    for sub_fdir in sub_fdirs:
        subdir_basename = os.path.basename(sub_fdir.root_name)
        # if this is a solution subdir (has multiple project descendents), add it to the list
        if len(sub_fdir.proj_descendents) > 0:
            sln_descendents.append(subdir_basename)
        # don't count header files
        elif len(sub_fdir.source_files) > 0:
            proj_descendents.append(subdir_basename)
    
    return Dir(start_dir, sub_fdirs, source_files, header_files, proj_descendents, sln_descendents)
    
def generate_pro_files(root_dir, parent_basename = ''):
    (basename, pro_text) = generate_pro_text(root_dir)
    
    if len(basename) == 0:  # if there shouldn't be a .pro file here
        return;  # don't process children

    pro_name = child_item(root_dir.root_name, basename + '.pro')
    pro_file = os.open(pro_name, os.O_WRONLY | os.O_CREAT | os.O_TRUNC)
    os.write(pro_file, pro_text)
    os.close(pro_file)
        
    # ---- Note: this is left out because all .pri's should now be checked
    #      in, so we want to see warnings if any are missing
    if CREATE_PRI_FILES:
        # make sure the .pri file exists to avoid qmake warnings
        pri_name = child_item(root_dir.root_name, basename + '.pri')
        try:
            pri_file = os.open(pri_name, os.O_RDONLY)
            os.close(pri_file)
        except:  # if the file doesn't exist, create it
            pri_file = os.open(pri_name, os.O_WRONLY | os.O_CREAT)
            if len(parent_basename) > 0:
                pri_text = 'include( ../' + parent_basename + '.pri )\n'
                os.write(pri_file, pri_text)
            os.close(pri_file)
    
    for subfdir in root_dir.dirs:
        generate_pro_files(subfdir, basename)
        
# for debugging only
def print_dirs(root_dir, prefix):
    pinc = '  '
    print prefix + 'd ' + root_dir.root_name
    for source in root_dir.source_files:
        print prefix + pinc + 's ' + source
    for header in root_dir.header_files:
        print prefix + pinc + 'h ' + header
    for proj_d in root_dir.proj_descendents:
        print prefix + pinc + 'p ' + proj_d
    for subfdir in root_dir.dirs:
        print_dirs(subfdir, prefix + pinc)

start_fdir = map_files(start_dir)
generate_pro_files(start_fdir)
