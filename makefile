# get name of this directory
THISDIR=$(shell \pwd | sed 's%^.*/%%')

# fortran compiler
FC = gfortran
#
# general flags
compile = -c $<
output = -o $@
#
# options
options = -fimplicit-none -std=f2008
warnings = -W -Waliasing -Wall -Warray-temporaries -Wcharacter-truncation	-Wfatal-errors \
-Wextra -Wimplicit-interface -Wintrinsics-std -Wsurprising -Wuninitialized -pedantic
debug = -g -fbacktrace -ffpe-trap=invalid,zero,overflow,underflow,denormal
#
# additional options for gfortran v4.5 and later
options_new = -std=f2018
warnings_new = -Wconversion-extra -Wimplicit-procedure -Winteger-division -Wreal-q-constant	\
-Wuse-without-only -Wrealloc-lhs-all
debug_new = -fcheck=all
#
# concatenate options
options := $(options) $(options_new)
warnings := $(warnings) $(warnings_new)
debug := $(debug) $(debug_new)
#
# fortran compiler flags
FCFLAGS = $(includes) $(options) $(warnings) $(debug)
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
# build directories
BINDIR := bin
OBJDIR := obj
#
# source file lists
#
# program files (executable)
SRC.F77 = $(wildcard *.f)
SRC.F90 = $(wildcard *.f90)
# add SRCDIR if present
SRCDIR := src
ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
	SRC.F77 += $(wildcard $(SRCDIR)/*.f)
	SRC.F90 += $(wildcard $(SRCDIR)/*.f90)
endif
SRC = $(SRC.F77) $(SRC.F90)
#
# directory for "include" files (not executable, not compilable)
INCDIR := inc
# add INCDIR if present
ifneq ("$(strip $(wildcard $(INCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(INCDIR)))
	includes = $(patsubst %,-I %,$(INCDIR))
	INCS.F77 = $(wildcard $(INCDIR)/*.f)
	INCS.F90 = $(wildcard $(INCDIR)/*.f90)
	INCS. +=  $(patsubst $(INCDIR)/%.f, %, $(INCS.F77)) \
	$(patsubst $(INCDIR)/%.f90, %, $(INCS.F90))
endif
#
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
	includes:=$(includes) -J $(MODDIR)
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
OBJS.F77 = $(SRC.F77:.f=.o)
OBJS.F90 = $(SRC.F90:.f90=.o)
OBJS.all = $(OBJS.F77) $(OBJS.F90)
OBJS.all := $(OBJS.all:$(SRCDIR)/%=%)
#
DEPS.o = $(addsuffix .o,$(DEPS.))
OBJS.o = $(filter-out $(DEPS.o),$(OBJS.all))
DEPS := $(addprefix $(OBJDIR)/,$(DEPS.o))
OBJS := $(addprefix $(OBJDIR)/,$(OBJS.o))
#
# executables
TARGET = 
EXES = $(addprefix $(BINDIR)/,$(OBJS.o:.o=))
#
# sub-programs
SUBDIRS :=
#
# recipes
all: $(EXES) $(SUBDIRS)
	@/bin/echo -e "$${TAB}$(THISDIR) $@ done"
$(SUBDIRS):
	@$(MAKE) --no-print-directory -C $@
printvars:
	@echo
	@echo "printing variables..."
	@echo "----------------------------------------------------"
	@echo
	@echo "includes = '$(includes)'"
	@echo "VPATH = '$(VPATH)'"

	@echo
	@echo "----------------------------------------------------"
	@echo


	@echo "SUBDIRS = $(SUBDIRS)"

	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "SRC.F77 = $(SRC.F77)"
	@echo
	@echo "SRC.F90 = $(SRC.F90)"
	@echo
	@echo "SRC = $(SRC)"
	@echo
	@echo "OBJS.all = $(OBJS.all)"

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

	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "OBJS.o = $(OBJS.o)"
	@echo
	@echo "DEPS.o = $(DEPS.o)"
	@echo
	@echo "MODS.mod = $(MODS.mod)"

	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "EXES = $(EXES)"
	@echo
	@echo "OBJS = $(OBJS)"
	@echo
	@echo "DEPS = $(DEPS)"
	@echo
	@echo "MODS = $(MODS)"
	@echo
	@echo "----------------------------------------------------"
	@echo "$@ done"
	@echo
#
# specific recipes

#
# generic recipes
$(BINDIR)/%: $(OBJDIR)/%.o $(DEPS) | $(BINDIR)
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
#
# recipes without outputs
.PHONY: all $(SUBDIRS) mostlyclean clean force out realclean distclean reset
#
# clean up
optSUBDIRS = $(addprefix $(MAKE) $@ --no-print-directory -C ,$(addsuffix ;,$(SUBDIRS)))
RM = @rm -vfrd
mostlyclean:
# remove compiled binaries
	@echo "removing compiled binary files..."
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
	$(RM) *.exe
	$(RM) *.out
	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"
force: clean
# force re-make
	@$(MAKE) --no-print-directory
out:
# remove outputs produced by executables
	@/bin/echo -e "\nremoving output files..."
	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"
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
test: distclean printvars all
	@echo "$(THISDIR) $@ done"
