# Makefile for NSPE extras/ comparison utilities
# Compiles tldiff, prsdiff, and tsdiff
#
# Usage:
#   make          - build all programs
#   make tldiff   - build tldiff only
#   make clean    - remove compiled files
#
# See README.md for program usage.

# compiler
your_f77 = gfortran

# compile flags
fflags = -O2 -c $< -o $@

# link flags
lflags = $^ -o $@

# programs
PROGRAMS = tldiff prsdiff tsdiff

all: $(PROGRAMS)

# module (shared dependency)
file_diff_utils.o: file_diff_utils.f90
	$(your_f77) $(fflags)

# programs
tldiff.o: tldiff.f90 file_diff_utils.o
	$(your_f77) $(fflags)

prsdiff.o: prsdiff.f90 file_diff_utils.o
	$(your_f77) $(fflags)

tsdiff.o: tsdiff.f90 file_diff_utils.o
	$(your_f77) $(fflags)

# link
tldiff: tldiff.o file_diff_utils.o
	$(your_f77) $(lflags)

prsdiff: prsdiff.o file_diff_utils.o
	$(your_f77) $(lflags)

tsdiff: tsdiff.o file_diff_utils.o
	$(your_f77) $(lflags)

# clean
clean:
	rm -f *.o *.mod $(PROGRAMS)

.PHONY: all clean
