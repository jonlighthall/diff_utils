# A generic makefile for C/C++ programs
# Mar 2023 JCL
# Adapted from template by Thomas Daley (Sep 2021)

# get name of this directory
THISDIR=$(shell \pwd | sed 's%^.*/%%')
DIR.NAME=$(shell basename $$PWD)

# C compiler
CC = gcc
# C++ compiler
CXX = g++
# linker
LD = g++

# fortran compiler
FC = gfortran
#

# general flags
compile = -c $<
output = -o $@

#
# Fortran options
foptions = -fimplicit-none -std=f2008
fwarnings = -W -Waliasing -Wall -Warray-temporaries -Wcharacter-truncation	\
		-Wfatal-errors -Wextra -Wimplicit-interface -Wintrinsics-std -Wsurprising		\
		-Wuninitialized -pedantic
fdebug = -g -fbacktrace -ffpe-trap=invalid,zero,overflow,underflow,denormal
#
# additional options for gfortran v4.5 and later
foptions_new = -std=f2018
fwarnings_new = -Wconversion-extra -Wimplicit-procedure -Winteger-division	\
		-Wreal-q-constant -Wuse-without-only -Wrealloc-lhs-all
fdebug_new = -fcheck=all
#
# concatenate options
foptions := $(options) $(options_new)
fwarnings := $(warnings) $(warnings_new)
fdebug := $(debug) $(debug_new)
#
# fortran compiler flags
FCFLAGS = $(fincludes) $(foptions) $(fwarnings) $(fdebug)
F77.FLAGS = -fd-lines-as-comments
F90.FLAGS =
FC.COMPILE = $(FC) $(compile) $(FCFLAGS)
FC.COMPILE.o = $(FC.COMPILE) $(output) $(F77.FLAGS)
FC.COMPILE.o.f90 = $(FC.COMPILE) $(output) $(F90.FLAGS)
FC.COMPILE.mod = $(FC.COMPILE) -o $(OBJDIR)/$*.o $(F90.FLAGS)
#
# fortran linker flags
FLFLAGS = $(output) $^
FC.LINK = $(FC) $(FLFLAGS)
#

warnings = -Wall
debug = -g

# define macros
#macros = -D DEBUG

# include directories for C/C++ source files
includes = -I include -I src

# universal flags
UFLAGS = $(warnings) $(debug) $(macros) $(includes) $(output)

# C flags
CFLAGS =
# C++ flags
CXXFLAGS =
# dependency generation flags
DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJDIR)/$*.d
# compile C source
COMPILE.c = $(CC) $(CFLAGS) $(UFLAGS) $(compile)
# compile C++ source
COMPILE.cxx = $(CXX) $(CXXFLAGS) $(DEPFLAGS) $(UFLAGS) $(compile)

# linker flags
LDFLAGS =
# library flags
LDLIBS =

# link objects
LINK.o = $(LD) $(LDFLAGS) $(LDLIBS) $(UFLAGS) $^

# build directories
BINDIR := bin
OBJDIR := obj
#

# source file lists

# program driver files (correspond to executables)
MAINS.C = $(wildcard *.c)
MAINS.CPP = $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.cxx)
MAINS.F77 = $(wildcard *.f)
MAINS.F90 = $(wildcard *.f90)
# add driver directory, if present
MAIN_DIR := main
ifneq ("$(strip $(wildcard $(MAIN_DIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(MAIN_DIR)))
	MAINS.C += $(wildcard $(MAIN_DIR)/*.c)
	MAINS.CPP += $(wildcard $(MAIN_DIR)/*.cc $(MAIN_DIR)/*.cpp $(MAIN_DIR)/*.cxx)
	MAINS.F77 += $(wildcard $(MAIN_DIR)/*.f)
	MAINS.F90 += $(wildcard $(MAIN_DIR)/*.f90)
endif
MAINS.CPP := $(strip $(MAINS.CPP))
MAINS.F77 := $(strip $(MAINS.F77))
MAINS.F90 := $(strip $(MAINS.F90))
MAINS = $(strip $(MAINS.C) $(MAINS.CPP) $(MAINS.F77) $(MAINS.F90))
# exclude readme files from the main list
#MAINS := $(filter-out $(wildcard *readme*), $(MAINS))
#
# program files (executable)
SRCS.F77 = $(wildcard *.f)
SRCS.F90 = $(wildcard *.f90)

# source files (implementation, correspond to header files)
# add source directory, if present
SRCDIR := src
ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
	SRCS.F77 += $(wildcard $(SRCDIR)/*.f)
	SRCS.F90 += $(wildcard $(SRCDIR)/*.f90)
endif
SRC = $(SRCS.F77) $(SRCS.F90)
#
# directory for "include" files (not executable, not compilable)
INCDIR := inc
# add INCDIR if present
ifneq ("$(strip $(wildcard $(INCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(INCDIR)))
	fincludes = $(patsubst %,-I %,$(INCDIR))
	INCS.F77 = $(wildcard $(INCDIR)/*.f)
	INCS.F90 = $(wildcard $(INCDIR)/*.f90)
	INCS. +=  $(patsubst $(INCDIR)/%.f, %, $(INCS.F77)) \
	$(patsubst $(INCDIR)/%.f90, %, $(INCS.F90))

endif

# Initialize C/C++ source file lists
SRCS.C = $(wildcard $(SRCDIR)/*.c)
SRCS.CPP = $(wildcard $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)
# Note: No need to exclude uband_diff.cpp since it doesn't contain main()

SRCS.CPP := $(strip $(SRCS.CPP))
SRCS = $(strip $(SRCS.C) $(SRCS.CPP))

# module files
# fortran module complier flags
FC.COMPILE.mod = $(FC.COMPILE) -o $(OBJDIR)/$*.o $(F90.FLAGS)
# build directory for compiled modules
MODDIR := mod
# source directory
MODDIR.in := modules
# add MODDIR.in if present
ifneq ("$(strip $(wildcard $(MODDIR.in)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(MODDIR.in)))
	MODS.F77 = $(wildcard $(MODDIR.in)/*.f)
	MODS.F90 = $(wildcard $(MODDIR.in)/*.f90)
	MODS. +=  $(patsubst $(MODDIR.in)/%.f, %, $(MODS.F77)) \
	$(patsubst $(MODDIR.in)/%.f90, %, $(MODS.F90))
endif
# add additional modules
MODS. +=
# add MODDIR to includes if MODS. not empty
ifneq ("$(MODS.)","")
	fincludes:=$(fincludes) -J $(MODDIR)
endif
# build list of modules
MODS.mod = $(addsuffix .mod,$(MODS.))
MODS := $(addprefix $(MODDIR)/,$(MODS.mod))
#
# Add any external procudures below. Note: shared procedures should be included in a module
# unless written in a different language.
#
# function files
FUNDIR := functions
# add FUNDIR if present
ifneq ("$(strip $(wildcard $(FUNDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(FUNDIR)))
	FUNS.F77 = $(wildcard $(FUNDIR)/*.f)
	FUNS.F90 = $(wildcard $(FUNDIR)/*.f90)
	FUNS. +=  $(patsubst $(FUNDIR)/%.f, %, $(FUNS.F77)) \
	$(patsubst $(FUNDIR)/%.f90, %, $(FUNS.F90))
endif
# add additional fucntions
FUNS. +=
#
# subroutine files
SUBDIR := subroutines
# add SUBDIR if present
ifneq ("$(strip $(wildcard $(SUBDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SUBDIR)))
	SUBS.F77 = $(wildcard $(SUBDIR)/*.f)
	SUBS.F90 = $(wildcard $(SUBDIR)/*.f90)
	SUBS. +=  $(patsubst $(SUBDIR)/%.f, %, $(SUBS.F77)) \
	$(patsubst $(SUBDIR)/%.f90, %, $(SUBS.F90))
endif
# add additional subroutines
SUBS. +=
#
# concatonate procedure lists (non-executables)
DEPS. = $(MODS.) $(SUBS.) $(FUNS.)
#
# build object lists
OBJS.F77 = $(SRCS.F77:.f=.o)
OBJS.F90 = $(SRCS.F90:.f90=.o)
FORTRAN_OBJS.all = $(OBJS.F77) $(OBJS.F90)
FORTRAN_OBJS.all := $(FORTRAN_OBJS.all:$(SRCDIR)/%=%)
#
FORTRAN_DEPS.o = $(addsuffix .o,$(DEPS.))
FORTRAN_OBJS.o = $(filter-out $(FORTRAN_DEPS.o),$(FORTRAN_OBJS.all))
FORTRAN_DEPS := $(addprefix $(OBJDIR)/,$(FORTRAN_DEPS.o))

# object files
# replace source file extensions with object file extensions
OBJS.src = $(patsubst %.cxx,%.o,$(patsubst %.cpp,%.o,$(patsubst %.cc,%.o,$(patsubst %.c,%.o,$(SRCS)))))
# strip source directory from object list
# should be a list of .o files with no directory
OBJS.o := $(OBJS.src:$(SRCDIR)/%=%)
# add object directory
OBJS := $(addprefix $(OBJDIR)/,$(OBJS.o))

#
# executables
TARGET =
# EXES should be built from main programs, not all source objects
# EXES = $(addprefix $(BINDIR)/,$(FORTRAN_OBJS.o:.o=))
#
# sub-programs
SUBDIRS :=

# dependency files
DEPS := $(OBJS:.o=.d)

# strip file extensions from main files
EXECS.main = $(patsubst %.f90,%,$(patsubst %.f,%,$(patsubst %.cxx,%,$(patsubst %.cpp,%,$(patsubst %.cc,%,$(patsubst %.c,%,$(MAINS)))))))
EXECS.main := $(strip $(EXECS.main))
# strip main directory from executable list
# should be a list of executables with no directory
EXECS.list := $(EXECS.main:$(MAIN_DIR)/%=%)
# add executable directory
EXECS := $(addprefix $(BINDIR)/,$(EXECS.list))

.DEFAULT_GOAL = all
#
# recipes

$(SUBDIRS):
	@$(MAKE) --no-print-directory -C $@

.PHONY: all
all: $(OBJDIR) $(BINDIR) $(EXECS)
	@/bin/echo -e "$${TAB}$(THISDIR) $@ done"

printvars:
	@echo
	@echo "printing variables..."
	@echo "----------------------------------------------------"
	@echo

	@echo "includes = '$(includes)'"
	@echo "fincludes = '$(fincludes)'"

	@echo "VPATH = '$(VPATH)'"
	@echo
	@echo "----------------------------------------------------"
	@echo
	@echo "SUBDIRS = $(SUBDIRS)"

	@echo
	@echo "MAIN_DIR = $(MAIN_DIR)"
	@echo "MAINS.C   = $(MAINS.C)"
	@echo "MAINS.CPP = $(MAINS.CPP)"
	@echo "MAINS.F77 = $(MAINS.F77)"
	@echo "MAINS.F90 = $(MAINS.F90)"
	@echo "MAINS     = $(MAINS)"
	@echo "----------------------------------------------------"
	@echo

	@echo "SRCS.F77 = $(SRCS.F77)"
	@echo
	@echo "SRCS.F90 = $(SRCS.F90)"
	@echo
	@echo "SRC = $(SRC)"
	@echo
	@echo "FORTRAN_OBJS.all = $(FORTRAN_OBJS.all)"

	@echo "SRCDIR  = $(SRCDIR)"
	@echo "SRCS.C   = $(SRCS.C)"
	@echo "SRCS.CPP = $(SRCS.CPP)"
	@echo "SRCS     = $(SRCS)"
	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "INCS. = $(INCS.)"
	@echo
	@echo "MODS. = $(MODS.)"
	@echo
	@echo "SUBS. = $(SUBS.)"
	@echo
	@echo "FUNS. = $(FUNS.)"
	@echo
	@echo "DEPS. = $(DEPS.)"

	@echo "OBJS.src = $(OBJS.src)"
	@echo "FORTRAN_OBJS.o   = $(FORTRAN_OBJS.o)"
	@echo "OBJS     = $(OBJS)"
	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "OBJS.o = $(OBJS.o)"
	@echo
	@echo "FORTRAN_DEPS.o = $(FORTRAN_DEPS.o)"
	@echo
	@echo "MODS.mod = $(MODS.mod)"

	@echo "EXECS.main = $(EXECS.main)"
	@echo "EXECS.list = $(EXECS.list)"
	@echo "EXECS      = $(EXECS)"
	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "EXES = $(EXES)"
	@echo
	@echo "OBJS = $(OBJS)"
	@echo

	@echo "DEPS = $(DEPS)"
	@echo
	@echo "FORTRAN_DEPS = $(FORTRAN_DEPS)"
	@echo
	@echo "MODS = $(MODS)"
	@echo

	@echo "----------------------------------------------------"
	@echo "$@ done"
	@echo
#

# specific recipes

# =============================================================================
# Testing Configuration
# =============================================================================

# Test configuration
TESTDIR := tests
TESTBINDIR := $(BINDIR)/tests

# Google Test flags
GTEST_CFLAGS = -isystem /usr/include/gtest
GTEST_LIBS = -lgtest -lgtest_main -pthread

# Test sources
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/%.cpp=$(OBJDIR)/test_%.o)
# Single test executable
TEST_EXECUTABLE = $(TESTBINDIR)/run_tests

# Create test directory
$(TESTBINDIR):
	@mkdir -v $(TESTBINDIR)

# Test compilation rule
$(OBJDIR)/test_%.o: $(TESTDIR)/%.cpp | $(OBJDIR)
	@echo "compiling test object $@..."
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(GTEST_CFLAGS) $(includes) $(warnings) $(debug) -c $< -o $@

# Test linking rule - link all test objects with main library objects into single executable
$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(OBJS) | $(TESTBINDIR)
	@echo "linking test executable $@..."
	$(CXX) $(LDFLAGS) $^ $(GTEST_LIBS) -o $@

#
# generic recipes
$(BINDIR)/%: $(OBJDIR)/%.o $(FORTRAN_DEPS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic executable $@..."
	$(FC.LINK)
$(OBJDIR)/%.o: %.f $(MODS) | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic object $@..."
	$(FC.COMPILE.o)
$(OBJDIR)/%.o: %.f90 $(MODS) | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic f90 object $@..."
	$(FC.COMPILE.o.f90)
$(MODDIR)/%.mod: %.f | $(MODDIR)
	@/bin/echo -e "\ncompiling generic module $@..."
	$(FC.COMPILE.mod)
$(MODDIR)/%.mod: %.f90 | $(MODDIR)
	@/bin/echo -e "\ncompiling generic f90 module $@..."
	$(FC.COMPILE.mod)

# Rules to build (link) each executable
# Executables in the root directory
$(BINDIR)/%: %.c $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C (.c) executable $@..."
	$(LINK.o)
$(BINDIR)/%: %.cc $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cc) executable $@..."
	$(LINK.o)
$(BINDIR)/%: %.cpp $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cpp) executable $@..."
	$(LINK.o)
$(BINDIR)/%: %.cxx $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cxx) executable $@..."
	$(LINK.o)

# Executables in the main directory
$(BINDIR)/%: $(MAIN_DIR)/%.c $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C (.c) executable $@..."
	$(LINK.o)
$(BINDIR)/%: $(MAIN_DIR)/%.cc $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cc) executable $@..."
	$(LINK.o)
$(BINDIR)/%: $(MAIN_DIR)/%.cpp $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cpp) executable $@..."
	$(LINK.o)
$(BINDIR)/%: $(MAIN_DIR)/%.cxx $(OBJS) | $(BINDIR)
	@/bin/echo -e "\nlinking generic C++ (.cxx) executable $@..."
	$(LINK.o)

# Rules to compile source files into object files

$(OBJDIR)/%.o:	$(SRCDIR)/%.c | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C (.c) object $@..."
	$(COMPILE.c)
$(OBJDIR)/%.o:	$(SRCDIR)/%.cc | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cc) object $@..."
	$(COMPILE.cxx)
$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cpp) object $@..."
	$(COMPILE.cxx)
$(OBJDIR)/%.o:	$(SRCDIR)/%.cxx | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cxx) object $@..."
	$(COMPILE.cxx)

$(OBJDIR)/%.o:	%.c | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C (.c) object $@..."
	$(COMPILE.c)
$(OBJDIR)/%.o: %.cc | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cc) object $@..."
	$(COMPILE.cxx)
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cpp) object $@..."
	$(COMPILE.cxx)
$(OBJDIR)/%.o: %.cxx | $(OBJDIR)
	@/bin/echo -e "\ncompiling generic C++ (.cxx) object $@..."
	$(COMPILE.cxx)

# Include dependency files (ignore missing files with -)
-include $(DEPS)

#
# define directory creation
$(BINDIR):
	@mkdir -v $(BINDIR)
$(OBJDIR):
	@mkdir -v $(OBJDIR)

$(MODDIR):
ifeq ("$(wildcard $(MODS))",)
	@echo "no modules specified"
else
	@echo "creating $(MODDIR)..."
	@mkdir -v $(MODDIR)
endif

# keep intermediate object files
.SECONDARY: $(DEPS) $(OBJS) $(MODS)

diff: $(BINDIR)/uband_diff

#
# recipes without outputs

.PHONY: all $(SUBDIRS) mostlyclean clean force out realclean distclean reset

.PHONY: mostlyclean clean force out realclean distclean reset
#
# clean up

optSUBDIRS = $(addprefix $(MAKE) $@ --no-print-directory -C ,$(addsuffix ;,$(SUBDIRS)))

RM = @rm -vfrd
mostlyclean:
# remove compiled binaries
	@echo "removing compiled binary files..."
# remove build files
	$(RM) $(OBJS)
# remove dependency files
	$(RM) $(DEPS)
# remove remaining binaries
	$(RM) $(OBJDIR)/*.o
	$(RM) $(OBJDIR)
	$(RM) *.o *.obj

	$(RM) $(MODDIR)/*.mod
	$(RM) $(MODDIR)
	$(RM) *.mod
	$(RM) fort.*
	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"
clean: mostlyclean
# remove binaries and executables
	@/bin/echo -e "\nremoving compiled executable files..."
	$(RM) $(BINDIR)/*
	$(RM) $(BINDIR)

	@echo "$(DIR.NAME) $@ done"
	$(RM) *.exe
	$(RM) *.out

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

	@echo "$(DIR.NAME) $@ done"
force: clean
# force re-make
	@$(MAKE) --no-print-directory
out:
# remove outputs produced by executables
	@/bin/echo -e "\nremoving output files..."

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

	@echo "$(DIR.NAME) $@ done"
realclean: clean out
# remove binaries and outputs

	@$(optSUBDIRS)

distclean: realclean
# remove binaries, outputs, and backups
	@/bin/echo -e "\nremoving backup files..."
# remove Git versions
	$(RM) *.~*~
# remove Emacs backup files
	$(RM) *~ \#*\#
# clean sub-programs

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

	@echo "$(DIR.NAME) $@ done"
reset: distclean
# remove untracked files
	@/bin/echo -e "\nresetting repository..."
	git reset HEAD
	git stash
	git clean -f

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"
#
# test the makefile
ftest: distclean printvars all
	@echo "$(THISDIR) $@ done"

	@echo "$(DIR.NAME) $@ done"

# force rebuild
.PHONY: remake
remake:	clean all

# execute the program (runs first executable found)
.PHONY: run
run: all
	@if [ -n "$(word 1,$(EXECS))" ]; then \
		echo "Running $(word 1,$(EXECS))..."; \
		./$(word 1,$(EXECS)); \
	else \
		echo "No executables found to run"; \
	fi

# =============================================================================
# Test Targets
# =============================================================================

.PHONY: test tests clean-tests
test: tests
	@echo "Running all tests..."
	@echo "Running $(TEST_EXECUTABLE)..."
	@if $(TEST_EXECUTABLE); then \
		echo "PASSED: All tests"; \
	else \
		echo "FAILED: Some tests failed"; \
		exit 1; \
	fi

tests: $(TEST_EXECUTABLE)

clean-tests:
	@echo "removing test files..."
	$(RM) $(TESTBINDIR)
	$(RM) $(OBJDIR)/test_*
	@echo "test cleanup done"

# Add test cleanup to main clean target
clean: mostlyclean clean-tests

# =============================================================================
# Help Target
# =============================================================================

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - build all executables (default)"
	@echo "  clean     - remove all generated files"
	@echo "  remake    - clean then build all"
	@echo "  run       - build and run the first executable"
	@echo "  test      - build and run all unit tests"
	@echo "  tests     - build unit tests only"
	@echo "  clean-tests - remove test files only"
	@echo "  help      - show this help message"


# =============================================================================
case1: diff
	./bin/uband_diff example_data/pe.std1.pe01.ref.txt example_data/pe.std1.pe01.test.txt 0 1 0 1

case2: diff
	./bin/uband_diff example_data/pe.std2.pe01.ref.txt example_data/pe.std2.pe01.test.txt 0 1 0 1

case3: diff
	./bin/uband_diff example_data/pe.std3.pe01.ref.txt example_data/pe.std3.pe01.test.txt 0 3 1