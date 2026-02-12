# Makefile for NSPE extras/ comparison utilities
#
# This makefile adapts to its environment:
#   - Standalone:    builds only the .f90 diff tools (tldiff, prsdiff, tsdiff)
#   - In NSPE extras/: also builds existing .f utility programs
#   - Legacy extras/:  builds only the .f programs (no .f90 files present)
#
# Usage:
#   make           - build all programs found in this directory
#   make tldiff    - build a specific program
#   make clean     - remove all build artifacts

# ============================================================
# compiler and flags
# ============================================================
FC       = gfortran
FFLAGS   = -g -O2 -Wall -Wfatal-errors -fbacktrace -fcheck=all
F77FLAGS = -fd-lines-as-comments -fall-intrinsics

# ============================================================
# build directories
# ============================================================
BINDIR = bin
OBJDIR = obj
MODDIR = mod

# ============================================================
# source discovery
# ============================================================
SRC_F77 = $(wildcard *.f)
SRC_F90 = $(wildcard *.f90)

# ============================================================
# external modules (parent directory — used by .f programs)
# Only included when the source files actually exist in ../
# ============================================================
EXT_MOD_DIR    = ..
EXT_MODS_KNOWN = constants spherical_earth
EXT_MODS       = $(foreach m,$(EXT_MODS_KNOWN),\
                   $(if $(wildcard $(EXT_MOD_DIR)/$(m).f90),$(m)))

# ============================================================
# local module (shared by .f90 diff programs)
# Only included when file_diff_utils.f90 is present.
# ============================================================
LOCAL_MOD = $(if $(wildcard file_diff_utils.f90),file_diff_utils)

# ============================================================
# derived lists
# ============================================================
# all .f files are standalone programs
PROGS_F77 = $(basename $(SRC_F77))
# .f90 programs = all .f90 files minus the local module
PROGS_F90 = $(filter-out $(LOCAL_MOD),$(basename $(SRC_F90)))
PROGS     = $(PROGS_F77) $(PROGS_F90)
EXES      = $(addprefix $(BINDIR)/,$(PROGS))

EXT_MOD_FILES  = $(addprefix $(MODDIR)/,$(addsuffix .mod,$(EXT_MODS)))
EXT_OBJ_FILES  = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(EXT_MODS)))
LOCAL_MOD_FILE = $(if $(LOCAL_MOD),$(MODDIR)/$(LOCAL_MOD).mod)
LOCAL_OBJ_FILE = $(if $(LOCAL_MOD),$(OBJDIR)/$(LOCAL_MOD).o)
ALL_MOD_FILES  = $(EXT_MOD_FILES) $(LOCAL_MOD_FILE)
DEP_OBJ_FILES  = $(EXT_OBJ_FILES) $(LOCAL_OBJ_FILE)

# ============================================================
# targets
# ============================================================
all: $(EXES)
	@echo "extras all done ($(words $(PROGS)) programs)"

# shortcut: allow "make tldiff" instead of "make bin/tldiff"
$(PROGS): %: $(BINDIR)/%

# ============================================================
# external module rules (only when parent sources exist)
# ============================================================
ifneq ($(EXT_MODS),)

$(MODDIR)/constants.mod $(OBJDIR)/constants.o: \
    $(EXT_MOD_DIR)/constants.f90 | $(MODDIR) $(OBJDIR)
	$(FC) $(FFLAGS) -c $< -o $(OBJDIR)/constants.o -J $(MODDIR)

ifneq ($(filter spherical_earth,$(EXT_MODS)),)
$(MODDIR)/spherical_earth.mod $(OBJDIR)/spherical_earth.o: \
    $(EXT_MOD_DIR)/spherical_earth.f90 $(MODDIR)/constants.mod \
    | $(MODDIR) $(OBJDIR)
	$(FC) $(FFLAGS) -c $< -o $(OBJDIR)/spherical_earth.o -J $(MODDIR)
endif

endif

# ============================================================
# local module rule (only when file_diff_utils.f90 exists)
# ============================================================
ifneq ($(LOCAL_MOD),)
$(LOCAL_MOD_FILE) $(LOCAL_OBJ_FILE): $(LOCAL_MOD).f90 | $(MODDIR) $(OBJDIR)
	$(FC) $(FFLAGS) -c $< -o $(LOCAL_OBJ_FILE) -J $(MODDIR)
endif

# ============================================================
# compile rules
# ============================================================
$(OBJDIR)/%.o: %.f $(EXT_MOD_FILES) | $(OBJDIR)
	$(FC) $(FFLAGS) $(F77FLAGS) -c $< -o $@ -J $(MODDIR)

$(OBJDIR)/%.o: %.f90 $(ALL_MOD_FILES) | $(OBJDIR)
	$(FC) $(FFLAGS) -c $< -o $@ -J $(MODDIR)

# ============================================================
# link rule
# ============================================================
$(BINDIR)/%: $(OBJDIR)/%.o $(DEP_OBJ_FILES) | $(BINDIR)
	$(FC) $^ -o $@

# ============================================================
# directory creation
# ============================================================
$(BINDIR) $(OBJDIR) $(MODDIR):
	@mkdir -p $@

# keep intermediate .o files
.SECONDARY: $(addprefix $(OBJDIR)/,$(addsuffix .o,$(PROGS)))

# ============================================================
# clean
# ============================================================
.PHONY: all clean distclean force $(PROGS)

clean:
	@echo "cleaning build artifacts..."
	rm -rf $(OBJDIR) $(MODDIR) $(BINDIR)
	rm -f *.o *.mod

distclean: clean
	@echo "removing backup files..."
	rm -f *.~*~ *~ \#*\#

force: clean
	@$(MAKE) --no-print-directory
