# Thomas Daley
# September 13, 2021

# A generic build template for C/C++ programs

# C compiler
CC = gcc
# C++ compiler
CXX = g++
# linker
LD = g++

# general flags
compile = -c $<
output = -o $@

# C flags
CFLAGS =
# C++ flags
CXXFLAGS =
# C/C++ flags
warnings = -Wall
CPPFLAGS = $(warnings)
# dependency-generation flags
DEPFLAGS = -MMD -MP

# compile C/C++ source
COMPILE.cpp = $(DEPFLAGS) $(CPPFLAGS) $(compile) $(output)
# compile C source
COMPILE.c = $(CC)  $(CFLAGS) 
# compile C++ source
COMPILE.cxx = $(CXX) $(CXXFLAGS) $(COMPILE.cpp)

# linker flags
LDFLAGS =
# library flags
LDLIBS =

# link objects
LINK.o = $(LD) $(LDFLAGS) $(LDLIBS) $(output) $^

# build directories
BINDIR := bin
OBJDIR := obj
#
# source file lists
#
# program files (executable)
SRC.C = $(wildcard *.c)
SRC.CPP = $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.cxx)

# add SRCDIR if present
SRCDIR := src
ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
	SRC.C += $(wildcard $(SRCDIR)/*.c)
	SRC.CPP += $(wildcard $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)
endif
SRC.CPP := $(strip $(SRC.CPP))
SRC = $(strip $(SRC.C) $(SRC.CPP))

SOURCES := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)

OBJS.C = $(SRC.C:.c=.o)
OBJS.cc = $(SRC.CPP:.cc=.o)
OBJS.cpp = $(OBJS.cc:.cpp=.o)
OBJS.CPP = $(OBJS.cpp:.cxx=.o)
# concatonate object lists
OBJS.all = $(strip $(OBJS.C) $(OBJS.CPP))
# strip source directory from object list
# should be a list of .o files with no directory
OBJS.o := $(OBJS.all:$(SRCDIR)/%=%)
# add object directory
OBJS := $(addprefix $(OBJDIR)/,$(OBJS.o))

OBJECTS = \
	$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.c)) \
	$(patsubst $(SRCDIR)/%.cc, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cc)) \
	$(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cpp)) \
	$(patsubst $(SRCDIR)/%.cxx, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cxx))
OBJECTS := $(strip $(OBJECTS))

# include compiler-generated dependency rules
DEPENDS := $(OBJECTS:.o=.d)
-include $(DEPENDS)

#
# executables
TARGET = 
EXES = $(addprefix $(BINDIR)/,$(OBJS.o:.o=.exe))
EXE = app

.DEFAULT_GOAL = all

.PHONY: all
all: $(BINDIR)/$(EXE)

printvars:
	@echo
	@echo "printing variables..."
	@echo "----------------------------------------------------"
	@echo	
	@echo "VPATH = '$(VPATH)'"

	@echo
	@echo "----------------------------------------------------"
	@echo
	
	@echo "SRC.C   = $(SRC.C)"	
	@echo "SRC.CPP = $(SRC.CPP)"	
	@echo "SRC     = $(SRC)"	
	@echo "SOURCES = $(SOURCES)"

	@echo
	@echo "----------------------------------------------------"
	@echo
	
	@echo "OBJS.C   = $(OBJS.C)"
	@echo "OBJS.CPP = $(OBJS.CPP)"
	@echo "OBJS.all = $(OBJS.all)"
	@echo "OBJS.o   = $(OBJS.o)"
	@echo "OBJS     = $(OBJS)"
	@echo "OBJECTS  = $(OBJECTS)"

	@echo
	@echo "----------------------------------------------------"
	@echo
		
	@echo "DEPENDS = $(DEPENDS)"

	@echo
	@echo "----------------------------------------------------"
	@echo

	@echo "EXE = $(EXE)"
	@echo "TARGET = $(TARGET)"
	@echo "EXES = $(EXES)"
	
	@echo
	@echo "----------------------------------------------------"
	@echo "$@ done"
	@echo
	
$(BINDIR)/$(EXE): $(OBJECTS) | $(BINDIR) 
	$(LINK.o)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c | $(OBJDIR)
	$(COMPILE.c)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cc | $(OBJDIR)
	$(COMPILE.cxx)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp | $(OBJDIR)
	$(COMPILE.cxx)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cxx | $(OBJDIR)
	$(COMPILE.cxx)
#
# define directory creation
$(BINDIR):
	@mkdir -v $(BINDIR)
$(OBJDIR):
	@mkdir -v $(OBJDIR)
#
# recipes without outputs
.PHONY: mostlyclean clean force out realclean distclean reset
#
# clean up
RM = @rm -vfrd
mostlyclean:
# remove compiled binaries
	@echo "removing compiled binary files..."
# remove build files
	$(RM) $(OBJECTS)
	$(RM) $(DEPENDS)
# remove remaining binaries
	$(RM) $(OBJDIR)/*.o
	$(RM) $(OBJDIR)
	$(RM) *.o *.obj
	@echo "$(THISDIR) $@ done"
clean: mostlyclean
# remove binaries and executables
	@echo "\nremoving compiled executable files..."
# removue build files
	$(RM) $(BINDIR)/$(EXE)
# remove remaining binaries
	$(RM) $(BINDIR)/*.exe
	$(RM) $(BINDIR)
	$(RM) *.exe
	$(RM) *.out
	@echo "$(THISDIR) $@ done"
force: clean
# force re-make
	@$(MAKE) --no-print-directory
out:
# remove outputs produced by executables
	@echo "\nremoving output files..."
	@echo "$(THISDIR) $@ done"
realclean: clean out
# remove binaries and outputs

distclean: realclean
# remove binaries, outputs, and backups
	@echo "\nremoving backup files..."
# remove Git versions
	$(RM) *.~*~
# remove Emacs backup files
	$(RM) *~ \#*\#
# clean sub-programs
	@echo "$(THISDIR) $@ done"
reset: distclean
# remove untracked files
	@echo "\nresetting repository..."
	git reset HEAD
	git stash
	git clean -f
	@echo "$(THISDIR) $@ done"

# force rebuild
.PHONY: remake
remake:	clean $(BINDIR)/$(EXE)

# execute the program
.PHONY: run
run: $(BINDIR)/$(EXE)
	./$(BINDIR)/$(EXE)

# remove everything except source
.PHONY: reset
reset: realclean
