# Makefile Best Practices Guide

## 1. VARIABLE NAMING CONVENTIONS

### Standard GNU Make Variables (Always use these exact names)
```makefile
CC       # C compiler
CXX      # C++ compiler
FC       # Fortran compiler
LD       # Linker (often same as CC/CXX)
AR       # Archiver for static libraries
CFLAGS   # C compiler flags
CXXFLAGS # C++ compiler flags
FCFLAGS  # Fortran compiler flags
CPPFLAGS # Preprocessor flags (includes, defines)
LDFLAGS  # Linker flags (library paths, link options)
LDLIBS   # Libraries to link (-lmath, -lpthread, etc.)
```

### Directory Variables (Use consistent naming)
```makefile
SRCDIR   # Source directory (not SRC_DIR or srcdir)
INCDIR   # Include directory
OBJDIR   # Object file directory
BINDIR   # Binary/executable directory
TESTDIR  # Test directory
```

### Project-Specific Variables (Use UPPER_CASE with underscores)
```makefile
PROJECT_NAME
BUILD_TYPE
VERSION_MAJOR
LIB_SOURCES_CXX   # Descriptive, category-specific
MAIN_SOURCES_F90  # Clear what type of sources
```

### Avoid These Patterns (from your current makefile)
```makefile
# ❌ Bad - unclear, non-standard
foptions
fwarnings
fdebug
warnings (should be part of CFLAGS)
debug (should be part of CFLAGS)

# ❌ Bad - dots in variable names
F77.FLAGS
FC.COMPILE.o

# ✅ Better
FORTRAN_STD_FLAGS
FORTRAN_WARNING_FLAGS
FORTRAN_DEBUG_FLAGS
F77FLAGS
FC_COMPILE_CMD
```

## 2. FLAG ORGANIZATION BEST PRACTICES

### Group Flags by Category
```makefile
# Warning flags
C_WARNING_FLAGS := -Wall -Wextra -Wpedantic
CXX_WARNING_FLAGS := $(C_WARNING_FLAGS) -Wold-style-cast

# Debug flags
C_DEBUG_FLAGS := -g -ggdb3

# Optimization flags (separate by build type)
C_OPT_FLAGS_DEBUG := -O0
C_OPT_FLAGS_RELEASE := -O2 -DNDEBUG

# Then combine into standard variables
CFLAGS := $(C_WARNING_FLAGS) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS)
```

### Use Build Configuration
```makefile
BUILD_TYPE ?= DEBUG

ifeq ($(BUILD_TYPE),RELEASE)
    C_OPT_FLAGS := $(C_OPT_FLAGS_RELEASE)
else ifeq ($(BUILD_TYPE),DEBUG)
    C_OPT_FLAGS := $(C_OPT_FLAGS_DEBUG)
endif
```

## 3. FILE ORGANIZATION STRUCTURE

```makefile
#==============================================================================
# PROJECT CONFIGURATION
#==============================================================================
# Project info, version, etc.

#==============================================================================
# COMPILER CONFIGURATION
#==============================================================================
# CC, CXX, FC definitions

#==============================================================================
# DIRECTORY STRUCTURE
#==============================================================================
# SRCDIR, BINDIR, etc.

#==============================================================================
# COMPILER FLAGS - ORGANIZED BY CATEGORY
#==============================================================================
# Warning flags, debug flags, optimization, then combined

#==============================================================================
# SOURCE FILE DISCOVERY
#==============================================================================
# Find and organize source files

#==============================================================================
# BUILD TARGETS
#==============================================================================
# .PHONY targets, main rules

#==============================================================================
# COMPILATION RULES
#==============================================================================
# Pattern rules for compiling

#==============================================================================
# CLEANING TARGETS
#==============================================================================
# clean, distclean, etc.
```

## 4. MODERN MAKEFILE FEATURES TO USE

### Automatic Dependency Generation
```makefile
DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJDIR)/$*.d
DEPS := $(OBJECTS:.o=.d)
-include $(DEPS)
```

### Use := Instead of = for Performance
```makefile
# ✅ Good - expands once when defined
SOURCES := $(wildcard src/*.cpp)

# ❌ Slower - expands every time it's used
SOURCES = $(wildcard src/*.cpp)
```

### Silent by Default, Verbose on Request
```makefile
# Default: show what's being built
$(OBJDIR)/%.o: %.cpp
    @echo "Compiling: $<"
    $(CXX) $(CXXFLAGS) -c $< -o $@

# Enable verbose with make V=1
ifeq ($(V),1)
    Q :=
else
    Q := @
endif
```

## 5. WHAT TO FIX IN YOUR CURRENT MAKEFILE

### Immediate Issues:
1. **Rename variables** to follow GNU conventions:
   - `foptions` → `FORTRAN_STD_FLAGS`
   - `fwarnings` → `FORTRAN_WARNING_FLAGS`
   - `warnings` → part of `CFLAGS`
   - `debug` → part of `CFLAGS`

2. **Group flags** by category instead of scattering them

3. **Standardize directory variables**:
   - `MAIN_DIR` → `MAINDIR`
   - Keep `SRCDIR`, `BINDIR` as-is (these are good)

4. **Fix broken references**:
   - `foptions := $(options) $(options_new)` - `options` is undefined
   - Should be: `FORTRAN_STD_FLAGS := $(FORTRAN_STD_FLAGS_BASE) $(FORTRAN_STD_FLAGS_NEW)`

5. **Organize into logical sections** with clear headers

6. **Add help target** for usability

7. **Add build type support** (DEBUG/RELEASE/FAST)

## 6. TESTING YOUR IMPROVED MAKEFILE

Test the reorganized version:
```bash
# Test basic build
make

# Test different build types
make BUILD_TYPE=RELEASE
make BUILD_TYPE=DEBUG

# Test help and debugging
make help
make print-vars

# Test cleaning
make clean
make distclean
```

The key is **consistency**, **clarity**, and **following established conventions** that other developers will recognize immediately.
