# A makefile for Java programs
# Oct 2025 JCL
# Follows pattern of C++ and Fortran makefiles

# get name of this directory
THISDIR=$(shell \pwd | sed 's%^.*/%%')
DIR.NAME=$(shell basename $$PWD)

# Java compiler
JAVAC = javac
# Java runtime
JAVA = java

# Java compiler flags
JAVAFLAGS = -d $(CLASSDIR)

# build directories - output to root build/ directory
BINDIR := ../../build/bin
CLASSDIR := ../../build/classes

# source file lists

# program driver files (correspond to executables)
MAINS.JAVA = $(wildcard *.java)
# add driver directory, if present
MAIN_DIR := main
ifneq ("$(strip $(wildcard $(MAIN_DIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(MAIN_DIR)))
	MAINS.JAVA += $(wildcard $(MAIN_DIR)/*.java)
endif
MAINS.JAVA := $(strip $(MAINS.JAVA))
MAINS = $(strip $(MAINS.JAVA))

# source files (implementation)
SRCDIR := src
ifneq ("$(strip $(wildcard $(SRCDIR)))","")
	VPATH += $(subst $(subst ,, ),:,$(strip $(SRCDIR)))
	SRCS.JAVA = $(wildcard $(SRCDIR)/*.java)
endif
SRCS.JAVA := $(strip $(SRCS.JAVA))
SRCS = $(strip $(SRCS.JAVA))

# class files
CLASSES.src = $(patsubst %.java,%.class,$(SRCS))
CLASSES.o := $(CLASSES.src:$(SRCDIR)/%=%)
CLASSES := $(addprefix $(CLASSDIR)/,$(CLASSES.o))

# strip file extensions from main files
EXECS.main = $(patsubst %.java,%,$(MAINS))
EXECS.main := $(strip $(EXECS.main))
# strip main directory from executable list
EXECS.list := $(EXECS.main:$(MAIN_DIR)/%=%)
# Java "executables" are wrapper scripts in BINDIR
EXECS := $(addprefix $(BINDIR)/,$(EXECS.list))

# sub-programs
SUBDIRS :=

.DEFAULT_GOAL = all

# recipes

$(SUBDIRS):
	@$(MAKE) --no-print-directory -C $@

.PHONY: all
all: $(CLASSDIR) $(BINDIR) $(CLASSES) $(EXECS)
	@/bin/echo -e "$${TAB}$(THISDIR) $@ done"

printvars:
	@echo
	@echo "printing variables..."
	@echo "----------------------------------------------------"
	@echo
	@echo "VPATH = '$(VPATH)'"
	@echo
	@echo "SUBDIRS = $(SUBDIRS)"
	@echo
	@echo "MAIN_DIR   = $(MAIN_DIR)"
	@echo "MAINS.JAVA = $(MAINS.JAVA)"
	@echo "MAINS      = $(MAINS)"
	@echo "----------------------------------------------------"
	@echo
	@echo "SRCDIR    = $(SRCDIR)"
	@echo "SRCS.JAVA = $(SRCS.JAVA)"
	@echo "SRCS      = $(SRCS)"
	@echo
	@echo "----------------------------------------------------"
	@echo
	@echo "CLASSES.src = $(CLASSES.src)"
	@echo "CLASSES.o   = $(CLASSES.o)"
	@echo "CLASSES     = $(CLASSES)"
	@echo
	@echo "----------------------------------------------------"
	@echo
	@echo "EXECS.main = $(EXECS.main)"
	@echo "EXECS.list = $(EXECS.list)"
	@echo "EXECS      = $(EXECS)"
	@echo
	@echo "----------------------------------------------------"
	@echo "$@ done"
	@echo

# Generic recipes

# Compile Java source to class files
$(CLASSDIR)/%.class: %.java | $(CLASSDIR)
	@/bin/echo -e "\ncompiling Java class $@..."
	$(JAVAC) $(JAVAFLAGS) $<

$(CLASSDIR)/%.class: $(MAIN_DIR)/%.java | $(CLASSDIR)
	@/bin/echo -e "\ncompiling Java class $@..."
	$(JAVAC) $(JAVAFLAGS) $<

# Create wrapper scripts for Java programs
# These allow running like: ./build/bin/pi_gen_java
# instead of: cd build/classes && java pi_gen_java
$(BINDIR)/%: $(CLASSDIR)/%.class | $(BINDIR)
	@/bin/echo -e "\ncreating Java wrapper script $@..."
	@echo '#!/bin/bash' > $@
	@echo '# Auto-generated wrapper for Java class $*' >> $@
	@echo 'SCRIPT_DIR="$$(cd "$$(dirname "$${BASH_SOURCE[0]}")" && pwd)"' >> $@
	@echo 'CLASSDIR="$${SCRIPT_DIR}/../classes"' >> $@
	@echo 'cd "$${CLASSDIR}" && java $*' >> $@
	@chmod +x $@

# Define directory creation
$(BINDIR):
	@mkdir -pv $(BINDIR)
$(CLASSDIR):
	@mkdir -pv $(CLASSDIR)

# Keep intermediate class files
.SECONDARY: $(CLASSES)

# Recipes without outputs

.PHONY: all $(SUBDIRS) mostlyclean clean force out realclean distclean reset

# Clean up
optSUBDIRS = $(addprefix $(MAKE) $@ --no-print-directory -C ,$(addsuffix ;,$(SUBDIRS)))

RM = @rm -vfrd
mostlyclean:
	@echo "removing compiled class files..."
	$(RM) $(CLASSDIR)/*.class
	$(RM) $(CLASSDIR)
	$(RM) *.class

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

clean: mostlyclean
	@/bin/echo -e "\nremoving wrapper scripts..."
	@echo "Note: bin/ is shared with parent - removing only Java wrappers"
	$(RM) $(EXECS)

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

force: clean
	@$(MAKE) --no-print-directory

out:
	@/bin/echo -e "\nremoving output files..."

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

realclean: clean out
	@$(optSUBDIRS)

distclean: realclean
	@/bin/echo -e "\nremoving backup files..."
	$(RM) *.~*~
	$(RM) *~ \#*\#

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

reset: distclean
	@/bin/echo -e "\nresetting repository..."
	git reset HEAD
	git stash
	git clean -f

	@$(optSUBDIRS)
	@echo "$(THISDIR) $@ done"

ftest: distclean printvars all
	@echo "$(THISDIR) $@ done"

.PHONY: remake
remake:	clean all

.PHONY: run
run: all
	@if [ -n "$(word 1,$(EXECS))" ]; then \
		echo "Running $(word 1,$(EXECS))..."; \
		./$(word 1,$(EXECS)); \
	else \
		echo "No executables found to run"; \
	fi

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - build all Java programs (default)"
	@echo "  clean     - remove all generated files"
	@echo "  remake    - clean then build all"
	@echo "  run       - build and run the first program"
	@echo "  help      - show this help message"
