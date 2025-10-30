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

# general flags
compile = -c $<
output = -o $@

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
BINDIR := build/bin
OBJDIR := build/obj

# source file lists

# program driver files (correspond to executables)
MAINS.C = $(wildcard *.c)
MAINS.CPP = $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.cxx)
# add driver directory, if present
MAIN_DIR := main
ifneq ("$(strip $(wildcard $(MAIN_DIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(MAIN_DIR)))
	MAINS.C += $(wildcard $(MAIN_DIR)/*.c)
	MAINS.CPP += $(wildcard $(MAIN_DIR)/*.cc $(MAIN_DIR)/*.cpp $(MAIN_DIR)/*.cxx)
endif
MAINS.CPP := $(strip $(MAINS.CPP))
MAINS = $(strip $(MAINS.C) $(MAINS.CPP))

# source files (implementation, correspond to header files)
# add source directory, if present
SRCDIR := src
ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
endif

# Initialize C/C++ source file lists
SRCS.C = $(wildcard $(SRCDIR)/*.c)
SRCS.CPP = $(wildcard $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)

SRCS.CPP := $(strip $(SRCS.CPP))
SRCS = $(strip $(SRCS.C) $(SRCS.CPP))

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
SUBDIRS := src/cpp src/fortran src/java

# dependency files
DEPS := $(OBJS:.o=.d)

# strip file extensions from main files
EXECS.main = $(patsubst %.cxx,%,$(patsubst %.cpp,%,$(patsubst %.cc,%,$(patsubst %.c,%,$(MAINS)))))
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
all: $(SUBDIRS) $(OBJDIR) $(BINDIR) $(EXECS)
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
	@echo "MAINS     = $(MAINS)"
	@echo "----------------------------------------------------"
	@echo

	@echo "SRCDIR  = $(SRCDIR)"
	@echo "SRCS.C   = $(SRCS.C)"
	@echo "SRCS.CPP = $(SRCS.CPP)"
	@echo "SRCS     = $(SRCS)"
	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "OBJS.src = $(OBJS.src)"
	@echo "OBJS     = $(OBJS)"
	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "OBJS.o = $(OBJS.o)"
	@echo

	@echo "EXECS.main = $(EXECS.main)"
	@echo "EXECS.list = $(EXECS.list)"
	@echo "EXECS      = $(EXECS)"
	@echo
	@echo "----------------------------------------------------"
	@echo
	@echo "GTEST_AVAILABLE = $(GTEST_AVAILABLE)"
	@echo "TEST_TARGETS    = $(TEST_TARGETS)"
	@echo

	@echo "EXES = $(EXES)"
	@echo
	@echo "OBJS = $(OBJS)"
	@echo

	@echo "DEPS = $(DEPS)"
	@echo

	@echo "----------------------------------------------------"
	@echo "$@ done"
	@echo
#

# specific recipes

# =============================================================================
# Testing Configuration
# =============================================================================

# Tests are now in cpp/ subdirectory - delegate test commands there


#
# generic recipes

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
	@mkdir -pv $(BINDIR)
$(OBJDIR):
	@mkdir -pv $(OBJDIR)

# keep intermediate object files
.SECONDARY: $(DEPS) $(OBJS)

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
test:
	@echo "Delegating test command to cpp subdirectory..."
	@$(MAKE) test --no-print-directory -C src/cpp

tests:
	@echo "Delegating tests command to cpp subdirectory..."
	@$(MAKE) tests --no-print-directory -C src/cpp

clean-tests:
	@echo "Delegating clean-tests command to cpp subdirectory..."
	@$(MAKE) clean-tests --no-print-directory -C src/cpp

# Note: test cleanup is handled by cpp/makefile
clean: mostlyclean

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
	@echo "  test      - build and run all unit tests (requires libgtest-dev)"
	@echo "  tests     - build unit tests only (requires libgtest-dev)"
	@echo "  clean-tests - remove test files only"
	@echo "  help      - show this help message"

# =============================================================================
case1: diff
	./bin/uband_diff example_data/pe.std1.pe01.ref.txt example_data/pe.std1.pe01.test.txt 0 1 0 1

case2: diff
	./bin/uband_diff example_data/pe.std2.pe01.ref.txt example_data/pe.std2.pe01.test.txt 0 1 0 1

case3: diff
	./bin/uband_diff example_data/pe.std3.pe01.ref.txt example_data/pe.std3.pe01.test.txt 0 3 1