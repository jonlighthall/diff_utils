#!/bin/bash -u

# get starting time in nanoseconds
start_time=$(date +%s%N)

# load bash utilities
fpretty=${HOME}/config/.bashrc_pretty
if [ -e "$fpretty" ]; then
    source "$fpretty"
    set_traps
fi

# determine if script is being sourced or executed
if ! (return 0 2>/dev/null); then
    # exit on errors
    set -e
fi
print_source

# generate executables before linking
cbar "Start Compiling"
make
cbar "Done Compiling"

# set target and link directories
target_dir="${src_dir_phys}/bin"
link_dir=$HOME/bin

cbar "Start Making Links"
# list of files to be linked
ext=''
for my_link in cpddiff \
    prsdiff \
    tldiff \
    tsdiff \
    uband_diff; do

    # define target (source)
    target=${target_dir}/${my_link}${ext}

    # define link (destination)
    link=${link_dir}/${my_link}

    # create link
    decho "linking $target to $link..."
    do_link_exe "$target" "$link"
done

target_dir="${src_dir_phys}"
ext='.sh'
for my_link in process_in_files; do
    # define target (source)
    target=${target_dir}/${my_link}${ext}

    # define link (destination)
    link=${link_dir}/${my_link}

    # create link
    decho "linking $target to $link..."
    do_link_exe "$target" "$link"
done
cbar "Done Making Links"
