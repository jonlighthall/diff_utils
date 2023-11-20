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

# executable name
EXE = app


# C flags
CFLAGS =
# C++ flags
CXXFLAGS =
# C/C++ flags
CPPFLAGS = -Wall
# dependency-generation flags
DEPFLAGS = -MMD -MP
# linker flags
LDFLAGS =
# library flags
LDLIBS =

# build directories
BINDIR = bin
OBJDIR = obj

#
# source file lists

# program files (executable)
SRC.C = $(wildcard *.c)
SRC.CPP = $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.cxx)

# add SRCDIR if present
SRCDIR = src

ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
	SRC.C += $(wildcard $(SRCDIR)/*.c)
	SRC.CPP += $(wildcard $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)
endif
SRC = $(SRC.C) $(SRC.CPP)

SOURCES := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*.cc $(SRCDIR)/*.cpp $(SRCDIR)/*.cxx)

OBJECTS := \
	$(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.c)) \
	$(patsubst $(SRCDIR)/%.cc, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cc)) \
	$(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cpp)) \
	$(patsubst $(SRCDIR)/%.cxx, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cxx))

# include compiler-generated dependency rules
DEPENDS := $(OBJECTS:.o=.d)

# compile C source
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(compile) $(output)
# compile C++ source
COMPILE.cxx = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(compile) $(output)
# link objects
LINK.o = $(LD) $(LDFLAGS) $(LDLIBS) $(OBJECTS) $(output)

.DEFAULT_GOAL = all

.PHONY: all
all: $(BINDIR)/$(EXE)

$(BINDIR)/$(EXE): $(SRCDIR) $(OBJDIR) $(BINDIR) $(OBJECTS)
	$(LINK.o)

$(SRCDIR):
	mkdir -p $(SRCDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c
	$(COMPILE.c)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cc
	$(COMPILE.cxx)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cpp
	$(COMPILE.cxx)

$(OBJDIR)/%.o:	$(SRCDIR)/%.cxx
	$(COMPILE.cxx)

# force rebuild
.PHONY: remake
remake:	clean $(BINDIR)/$(EXE)

# execute the program
.PHONY: run
run: $(BINDIR)/$(EXE)
	./$(BINDIR)/$(EXE)

# remove previous build and objects
.PHONY: clean
clean:
	$(RM) $(OBJECTS)
	$(RM) $(DEPENDS)
	$(RM) $(BINDIR)/$(EXE)

# remove everything except source
.PHONY: reset
reset:
	$(RM) -r $(OBJDIR)
	$(RM) -r $(BINDIR)

-include $(DEPENDS)
