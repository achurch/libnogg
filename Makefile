#
# libnogg: a decoder library for Ogg Vorbis streams
# Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
#
# This software may be copied and redistributed under certain conditions;
# see the file "COPYING" in the source code distribution for details.
# NO WARRANTY is provided with this software.
#

#
# By default, all commands will be displayed in an abbreviated form, such
# as "Compiling foo.c", instead of the actual command lines executed.
# Pass V=1 on the make command line to display the actual commands instead.
#

###########################################################################
############################## Configuration ##############################
###########################################################################

# In addition to the options below, standard build environment variables
# (CC, CFLAGS, etc.) are also recognized.

#----------------------------- Build options -----------------------------#

# BUILD_SHARED:  If this variable is set to 1, the build process will
# create a shared library (typically "libnogg.so" on Unix-like systems).
# If the variable is set to 0, no shared library will not be built.
#
# The default is 1 (a shared library will be built), except when building
# with the Microsoft Visual C compiler.  Since MSVC creates import
# libraries for DLLs whose names collide with static libraries, only one
# library type can be built in a single call to "make"; thus, when using
# MSVC, this option defaults to 0, and attempting to set both BUILD_SHARED
# and BUILD_STATIC to 1 will cause an error.

BUILD_SHARED = $(if $(filter msvc,$(CC_TYPE)),0,1)


# BUILD_STATIC:  If this variable is set to 1, the build process will
# create a static library (typically "libnogg.a" on Unix-like systems).
# If the variable is set to 0, no static library will not be built.  Note
# that tests are linked against the static library, so "make test" will
# cause the static library to be built even if this variable is set to 0.
#
# It is possible, though mostly meaningless, to set both BUILD_SHARED and
# BUILD_STATIC to 0.  In this case, "make" will do nothing, and "make
# install" will install only the library header file.
#
# The default is 1 (a static library will be built).

BUILD_STATIC = 1


# BUILD_TOOLS:  If this variable is set to 1, the build process will also
# build the tool programs in the tools/ directory, creating tool
# executables in the top directory.  Note that at least one of BUILD_SHARED
# and BUILD_STATIC must be enabled for the tool programs to be built.
#
# The default is 0 (the tool programs will not be built).

BUILD_TOOLS = 1


# ENABLE_ASM_ARM_NEON:  If this variable is set to 1, optimized assembly
# code for the ARM platform using NEON instructions will be compiled into
# the library.
#
# The default is 0 (NEON assembly code will not be included).

ENABLE_ASM_ARM_NEON = 0


# ENABLE_ASM_X86_AVX2:  If this variable is set to 1, optimized assembly
# code for the x86 platform using AVX2 and FMA3 instructions will be
# compiled into the library.  If enabled, ENABLE_ASM_X86_SSE2 will also be
# implicitly enabled.
#
# Note that the library does not currently include support for switching
# between different optimized decoder implementations at runtime; if this
# setting is enabled, the decoder will fail at runtime on CPUs without
# both the AVX2 and FMA3 instruction set extensions.  (The library checks
# for the extensions when opening a stream, and will return the error
# VORBIS_ERROR_NO_CPU_SUPPORT at open time if the extensions are not
# supported by the runtime CPU.)
#
# The default is 0.

ENABLE_ASM_X86_AVX2 = 0


# ENABLE_ASM_X86_SSE2:  If this variable is set to 1, optimized assembly
# code for the x86 platform using SSE2 instructions will be compiled into
# the library.
#
# The default is 1 when building for an x86 platform, 0 otherwise.  (If the
# build target platform cannot be detected, the value will default to 0.)

ENABLE_ASM_X86_SSE2 = 0


# ENABLE_ASSERT:  If this variable is set to 1, additional assertion checks
# will be compiled into the code to guard against bugs in the library.
# This requires support for the assert() macro and <assert.h> header in the
# system's runtime libraries.
#
# The default is 0 (assertion checks will not be included).

ENABLE_ASSERT = 0


# INSTALL_PKGCONFIG:  If this variable is set to 1, the build process will
# install a control file for the "pkg-config" tool as
# "$(LIBDIR)/pkgconfig/nogg.pc".
#
# The default is 0 (nogg.pc will not be installed).

INSTALL_PKGCONFIG = 0


# TREMOR_SOURCE:  If this variable is set to the pathname of a directory
# containing the Tremor (http://svn.xiph.org/trunk/Tremor/) source code,
# the nogg-benchmark tool will include support for benchmarking against
# Tremor as well as libvorbis.
#
# The default is to not include Tremor benchmarking support.

TREMOR_SOURCE =


# USE_LOOKUP_TABLES:  If this variable is set to 1, the library will
# include precomputed lookup tables for decoding.  This slightly reduces
# initialization time per stream at the cost of an additional 120kB of
# static memory use.
#
# The default is 1 (lookup tables will be enabled).

USE_LOOKUP_TABLES = 1


# USE_STDIO:  If this variable is set to 1, the library will include
# support for reading files from the filesystem using C stdio.  If the
# variable is set to 0, this support will be disabled, and the library
# will not reference any stdio functions.
#
# The default is 1 (stdio support will be included).

USE_STDIO = 1


# WARNINGS_AS_ERRORS:  If this variable is set to 1, the build will abort
# if the compiler emits any warnings.
#
# The default is 0 (warnings will not abort the build).

WARNINGS_AS_ERRORS = 0

#----------------------- Installation target paths -----------------------#

# BINDIR:  Sets the directory into which the tool programs (nogg-benchmark
# and nogg-decode) will be installed.  This path is not used if BUILD_TOOLS
# is set to 0.
#
# The default is "$(PREFIX)/bin".

BINDIR = $(PREFIX)/bin


# DESTDIR:  Sets the location of the installation root directory.  This
# string, if any, is prefixed to all target pathnames during installation,
# but it is not included in pathnames written to installed files, allowing
# the package to be installed to a non-root directory hierarchy.
#
# The default is the empty string.

DESTDIR =


# INCDIR:  Sets the directory into which the library's header file
# (nogg.h) will be installed.
#
# The default is "$(PREFIX)/include".

INCDIR = $(PREFIX)/include


# LIBDIR:  Sets the directory into which the shared and static library
# files will be installed.
#
# The default is "$(PREFIX)/lib".

LIBDIR = $(PREFIX)/lib


# PREFIX:  Sets the base directory for installation paths.  This is only
# used to set the default paths for BINDIR, INCDIR, and LIBDIR, and to set
# the "prefix" tag in the pkg-config control file if it is installed.
#
# The default is "/usr/local".

PREFIX = /usr/local

###########################################################################
############################## Internal data ##############################
###########################################################################

#-------------------------------------------------------------------------#
# Note: You should never need to modify any variables below this point in #
# the file.                                                               #
#-------------------------------------------------------------------------#

# Package name:
PACKAGE = nogg

# Library version:
VERSION = 1.17
VERSION_MAJOR = $(firstword $(subst ., ,$(VERSION)))

# Library output filenames: (note that $(*_EXT) are set below)
SHARED_LIB = lib$(PACKAGE)$(SHARED_EXT)
STATIC_LIB = lib$(PACKAGE)$(STATIC_EXT)

# Source and object filenames:
LIBRARY_SOURCES := $(sort $(wildcard src/*/*.c))
LIBRARY_OBJECTS = $(LIBRARY_SOURCES:%.c=%$(OBJ_EXT))
TEST_SOURCES := $(sort $(wildcard tests/*/*.c))
TEST_OBJECTS = $(TEST_SOURCES:%.c=%$(OBJ_EXT))
TEST_BINS = $(TEST_SOURCES:%.c=%$(EXE_EXT))
TOOL_SOURCES := $(sort $(wildcard tools/nogg-*.c))
TOOL_BINS = $(TOOL_SOURCES:tools/%.c=%$(EXE_EXT))

###########################################################################
#################### Helper functions and definitions #####################
###########################################################################

# if-true:  Return the second parameter if the variable named by the first
# parameter has the value 1, the third parameter (which may be omitted)
# otherwise.

if-true = $(if $(filter 1,$($1)),$2,$3)


# define-if-true:  Return an appropriate -D compiler option for the given
# variable name if its value is 1, the empty string otherwise.

define-if-true = $(call if-true,$1,$(CFLAG_DEFINE)$1)


# ECHO:  Expands to "@:" (a no-op) in verbose (V=1) mode, "@echo" in
# non-verbose mode.  Used for displaying abbreviated command text.

ECHO = $(call if-true,V,@:,@echo)


# Q:  Expands to the empty string in verbose (V=1) mode, "@" otherwise.
# Used to hide output of command lines in non-verbose mode.

Q = $(call if-true,V,,@)

###########################################################################
############################# Toolchain setup #############################
###########################################################################

# Default tool program names:

CC = cc
AR = ar
RANLIB = ranlib
GCOV = $(error gcov program unknown for this compiler)

# "ar" with "rcu" option word and a space.  Used to let MSVC's lib.exe
# work correctly, since it requires no space before the output filename.
# $(preserve-space) expands to the null string, and is used to ensure that
# the trailing space is included in the variable value.
AR_rcu_ = $(AR) rcu $(preserve-space)


# Try and guess what sort of compiler we're using, so we can set
# appropriate default options.

CC_TYPE = unknown
ifneq (,$(filter clang%,$(subst -, ,$(CC))))
    CC_TYPE = clang
else ifneq (,$(filter icc%,$(subst -, ,$(CC))))
    CC_TYPE = icc
else ifneq (,$(filter gcc%,$(subst -, ,$(CC))))
    CC_TYPE = gcc
else ifneq (,$(filter cl.exe%,$(lastword $(subst /, ,$(subst \, ,$(CC))))))
    CC_TYPE = msvc
else
    CC_VERSION_TEXT := $(shell $(CC) --version 2>&1)
    ifneq (,$(filter clang LLVM,$(CC_VERSION_TEXT)))
        CC_TYPE = clang
    else ifneq (,$(filter gcc GCC,$(CC_VERSION_TEXT)))
        CC_TYPE = gcc
    else ifneq (,$(and $(filter Microsoft,$(CC_VERSION_TEXT)),$(filter Optimizing,$(CC_VERSION_TEXT))))
        # "Microsoft (R) C/C++ Optimizing Compiler"
        CC_TYPE = msvc
    else ifneq (__GNUC__,$(shell echo __GNUC__ | $(CC) -E - 2>/dev/null | tail -1))
        # GCC invoked as "cc" may not have any "gcc" in --version output,
        # so treat a compiler whose preprocessor recognizes __GNUC__ (and
        # thus translates it to something else) as GCC.
        CC_TYPE = gcc
    endif
endif

CFLAG_COMPILE = -c
CFLAG_DEFINE = -D
CFLAG_INCLUDE_DIR = -I
CFLAG_INCLUDE_FILE = -include $(preserve-space)
CFLAG_NOOPT = -O0
CFLAG_OUTPUT = -o
CFLAG_OUTPUT_EXE = -o
CC-autodependency-flags = -MMD -MF '$1'
EXE_EXT =
OBJ_EXT = .o
STATIC_EXT = .a
SHARED_EXT = .$(if $(filter darwin%,$(OSTYPE)),dylib,$(if $(filter mingw%,$(ARCH)),dll,so))

ifeq ($(CC_TYPE),clang)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -pedantic -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wpointer-arith -Wshadow -Wwrite-strings \
        -Wundef -Wno-unused-parameter -Wvla \
        $(call if-true,ENABLE_ASM_ARM_NEON,-mfpu=neon) \
        $(call if-true,ENABLE_ASM_X86_AVX2,-msse -msse2 -mavx -mavx2 -mfma,$(call if-true,ENABLE_ASM_X86_SSE2,-msse -msse2))
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
    BASE_LDFLAGS =
    TEST_CFLAGS =
    GCOV = llvm-cov
    GCOV_OPTS = -b -c
    GCOV_FILE_OPTS = \
        -gcno="`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.gcno|'`" \
        -gcda="`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.gcda|'`" \
        -o "`echo "$1" | sed -e 's|[/.]|-|g'`.out"
else ifeq ($(CC_TYPE),gcc)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wlogical-op -Wpointer-arith -Wshadow \
        -Wwrite-strings -Wundef -Wno-unused-parameter -Wvla \
        $(call if-true,ENABLE_ASM_ARM_NEON,-mfpu=neon) \
        $(call if-true,ENABLE_ASM_X86_AVX2,-msse -msse2 -mavx -mavx2 -mfma,$(call if-true,ENABLE_ASM_X86_SSE2,-msse -msse2))
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 -pedantic \
        -Wmissing-declarations -Wstrict-prototypes
    BASE_LDFLAGS =
    TEST_CFLAGS =
    GCOV = gcov >/dev/null
    GCOV_OPTS = -b -c -l -p
    GCOV_FILE_OPTS = -o "`echo \"$1\" | sed -e 's|\.[^./]*$$|_cov.o|'`" '$1'
else ifeq ($(CC_TYPE),icc)
    BASE_FLAGS = -O2 -g -I. \
        $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wpointer-arith -Wreturn-type -Wshadow -Wuninitialized \
        -Wunknown-pragmas -Wunused-function -Wunused-variable -Wwrite-strings
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
    BASE_LDFLAGS =
    TEST_CFLAGS =
else ifeq ($(CC_TYPE),msvc)
    ifneq (,$(and $(filter 1,$(BUILD_SHARED)),$(filter 1,$(BUILD_STATIC))))
        $(error Cannot build shared and static libraries at the same time using MSVC)
    endif
    CFLAG_COMPILE = /c
    CFLAG_DEFINE = /D
    CFLAG_INCLUDE_DIR = /I
    CFLAG_NOOPT = /Od
    CFLAG_OUTPUT = /Fo
    CFLAG_OUTPUT_EXE = /out:
    CC-autodependency-flags =
    EXE_EXT = .exe
    OBJ_EXT = .obj
    STATIC_EXT = .lib
    SHARED_EXT = .dll
    expand-paths = $(foreach i,$(subst ;, ,$(subst $(preserve-space) ,$(shell echo -ne '\1'),$2)),$1'$(subst $(shell echo -ne '\1'), ,$(subst ','\'',$i))')
    # Ideally we want to disable extensions (/Za), but that also breaks C99
    # compound literals.  It's been *twenty-one years*, Microsoft...
    BASE_FLAGS = /utf-8 /O2 /fp:precise /GF /GS /sdl /I. \
        $(call expand-paths,/I,$(INCLUDE)) \
        /D_CRT_DECLARE_NONSTDC_NAMES=0 \
        /W3 $(call if-true,WARNINGS_AS_ERRORS,/WX) \
        /FC /diagnostics:column /nologo
    # Suppress fopen_s() advertising diagnostic.
    BASE_FLAGS += /wd4996
    BASE_CFLAGS = $(BASE_FLAGS)
    BASE_LDFLAGS = /link /subsystem:console /dynamicbase /nxcompat \
        $(call expand-paths,/libpath:,$(LIB)) \
        /nologo
    TEST_CFLAGS = /wd4244 /wd4267 /wd4305
    AR_rcu_ = $(subst cl.exe,lib.exe,$(CC)) /nologo /out:
    RANLIB = :
else
    $(warning *** Warning: Unknown compiler type.)
    $(warning *** Make sure your CFLAGS are set correctly!)
endif


# Update build parameter defaults based on the compiler's target platform.

ifneq ($(filter clang gcc icc,$(CC_TYPE)),)
    TARGET := $(subst -, ,$(shell $(CC) -dumpmachine))
    ARCH := $(or $(firstword $(TARGET)),unknown)
    OSTYPE := $(or $(word 3,$(TARGET)),unknown)
else
    ARCH := unknown
    OSTYPE := unknown
endif

ifneq ($(filter i386 x86_64,$(ARCH)),)
    ENABLE_ASM_X86_SSE2 = 1
endif

ifneq ($(filter darwin%,$(OSTYPE)),)
    SHARED_LIB_FULLNAME = $(subst .dylib,.$(VERSION).dylib,$(SHARED_LIB))
    SHARED_LIB_LINKNAME = $(subst .dylib,.$(VERSION_MAJOR).dylib,$(SHARED_LIB))
    SHARED_LIB_LDFLAGS = \
        -dynamiclib \
        -install_name '$(LIBDIR)/$(SHARED_LIB_FULLNAME)' \
        -compatibility_version $(firstword $(subst ., ,$(VERSION))) \
        -current_version $(VERSION) \
        -Wl,-single_module
else ifeq ($(CC_TYPE),msvc)
    SHARED_LIB_FULLNAME = $(SHARED_LIB)
    SHARED_LIB_LDFLAGS = $(BASE_LDFLAGS) /dll /implib:'$(STATIC_LIB)' \
        $(foreach i,$(shell grep '^extern .*([^)]*' include/nogg.h | sed -e 's/^extern .*[^A-Za-z0-9_]\([A-Za-z_][A-Za-z0-9_]*\)([^)]*.*/\1/'),/export:$i)
    # The [^)]* in the above regular expressions is meaningless, but needed
    # to avoid confusing Make's parenthesis matching.
else ifneq ($(filter mingw%,$(ARCH)),)
    SHARED_LIB_LDFLAGS = \
        -shared \
        -Wl,--enable-auto-image-base \
        -Xlinker --out-implib -Xlinker '$@.a'
else
    SHARED_LIB_FULLNAME = $(SHARED_LIB).$(VERSION)
    SHARED_LIB_LINKNAME = $(SHARED_LIB).$(VERSION_MAJOR)
    SHARED_LIB_CFLAGS = -fPIC
    SHARED_LIB_LDFLAGS = -shared -Wl,-soname=$(SHARED_LIB_LINKNAME)
endif


# Final flag set.  Note that the user-specified $(CFLAGS) reference comes
# last so the user can override any of our default flags.

ALL_DEFS = $(strip \
    $(call define-if-true,ENABLE_ASM_ARM_NEON) \
    $(call define-if-true,ENABLE_ASM_X86_AVX2) \
    $(call define-if-true,ENABLE_ASM_X86_SSE2) \
    $(call define-if-true,ENABLE_ASSERT) \
    $(call define-if-true,USE_LOOKUP_TABLES) \
    $(call define-if-true,USE_STDIO) \
    $(CFLAG_DEFINE)VERSION=\"$(VERSION)\")

ALL_CFLAGS = $(BASE_CFLAGS) $(ALL_DEFS) $(CFLAGS)


# Libraries to use when linking tool and test programs.

LIBS = $(if $(filter msvc,$(CC_TYPE)),,-lm)

###########################################################################
############################### Build rules ###############################
###########################################################################

#----------------------------- Entry points ------------------------------#

.PHONY: all all-shared all-static all-tools
.PHONY: install install-headers install-pc install-shared install-static install-tools
.PHONY: test clean spotless


all: $(call if-true,BUILD_SHARED,all-shared) \
     $(call if-true,BUILD_STATIC,all-static) \
     $(call if-true,BUILD_TOOLS,all-tools)

all-shared: $(SHARED_LIB)

all-static: $(STATIC_LIB)

all-tools: $(TOOL_BINS)


install: $(call if-true,BUILD_SHARED,install-shared) \
         $(call if-true,BUILD_STATIC,install-static) \
         install-headers \
         $(call if-true,INSTALL_PKGCONFIG,install-pc) \
         $(call if-true,BUILD_TOOLS,install-tools)

install-headers:
	$(ECHO) 'Installing header files'
	$(Q)mkdir -p '$(DESTDIR)$(INCDIR)'
	$(Q)cp -pf include/nogg.h '$(DESTDIR)$(INCDIR)/'

install-pc:
	$(ECHO) 'Installing pkg-config control file'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)/pkgconfig'
	$(Q)sed \
	    -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@INCDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(INCDIR))|g' \
	    -e 's|@LIBDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(LIBDIR))|g' \
	    -e 's|@VERSION@|$(VERSION)|g'\
	    <$(PACKAGE).pc.in >'$(DESTDIR)$(LIBDIR)/pkgconfig/$(PACKAGE).pc'

install-shared: all-shared
	$(ECHO) 'Installing shared library'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)'
	$(Q)cp -pf $(SHARED_LIB) '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB_FULLNAME)'
	$(Q)$(if $(SHARED_LIB_LINKNAME),ln -sf '$(SHARED_LIB_FULLNAME)' '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB_LINKNAME)')
	$(Q)$(if $(filter-out $(SHARED_LIB),$(SHARED_LIB_FULLNAME)),ln -sf '$(SHARED_LIB_FULLNAME)' '$(DESTDIR)$(LIBDIR)/$(SHARED_LIB)')

install-static: all-static
	$(ECHO) 'Installing static library'
	$(Q)mkdir -p '$(DESTDIR)$(LIBDIR)'
	$(Q)cp -pf $(STATIC_LIB) '$(DESTDIR)$(LIBDIR)/'

install-tools: all-tools
	$(ECHO) 'Installing tool programs'
	$(Q)mkdir -p '$(DESTDIR)$(BINDIR)'
	$(Q)cp -pf $(TOOL_BINS) '$(DESTDIR)$(BINDIR)/'


# Sanity check to ensure that "eval $${test}" won't do anything unexpected.
override TEST_BINS := $(TEST_BINS)
ifneq (,$(findstring ',$(TEST_BINS)))
    $(error invalid character in TEST_BINS)
endif
ifneq (,$(shell echo '$(TEST_BINS)' | grep '[^A-Za-z0-9_/ -]'))
    $(error invalid character in TEST_BINS)
endif

test: $(TEST_BINS)
	$(ECHO) 'Running tests'
	$(Q)ok=0 ng=0; \
	    for test in $^; do \
	        $(call if-true,V,echo "+ $${test}";) \
	        if eval $${test}; then \
	            ok=`expr $${ok} + 1`; \
	        else \
	            ng=`expr $${ng} + 1`; \
	        fi; \
	    done; \
	    if test $${ng} = 0; then \
	        echo 'All tests passed.'; \
	    else \
	        if test $${ok} = 1; then ok_s=''; else ok_s='s'; fi; \
	        if test $${ng} = 1; then ng_s=''; else ng_s='s'; fi; \
	        echo "$${ok} test$${ok_s} passed, $${ng} test$${ng_s} failed."; \
	        exit 1; \
	    fi

coverage: tests/coverage
	$(ECHO) 'Running tests'
	$(Q)find src -name \*.gcda -exec rm '{}' +
	$(Q)tests/coverage $(call if-true,V,-v)
	$(ECHO) 'Collecting coverage information'
	$(Q)rm -rf .covtmp
	$(Q)mkdir .covtmp
	$(Q)ln -s ../src .covtmp/
	$(Q)set -e; cd .covtmp && for f in $(LIBRARY_SOURCES); do \
	    $(call if-true,V,echo + $(GCOV) $(GCOV_OPTS) $(call GCOV_FILE_OPTS,$$f) $(GCOV_STDOUT);) \
	    $(GCOV) $(GCOV_OPTS) $(call GCOV_FILE_OPTS,$$f) $(GCOV_STDOUT); \
	done
	$(Q)rm -f .covtmp/src
	$(Q)cat .covtmp/* >coverage.txt
	$(Q)rm -rf .covtmp
	$(ECHO) 'Coverage results written to $(abspath coverage.txt)'


clean:
	$(ECHO) 'Removing object and dependency files'
	$(Q)find src tests tools \( -name '*$(OBJ_EXT)' -o -name '*.d' -o -name \*.d.tmp \) -exec rm '{}' +
	$(Q)rm -f tests/coverage-tests.h
	$(ECHO) 'Removing test executables'
	$(Q)rm -f $(TEST_BINS) tests/coverage
	$(ECHO) 'Removing coverage data files'
	$(Q)find src tests tools \( -name \*.gcda -o -name \*.gcno \) -exec rm '{}' +
	$(Q)rm -rf .covtmp

spotless: clean
	$(ECHO) 'Removing executable and library files'
	$(Q)rm -f $(SHARED_LIB) $(STATIC_LIB) $(TOOL_BINS)
	$(ECHO) 'Removing coverage output'
	$(Q)rm -f coverage.txt

#-------------------------- Library build rules --------------------------#

ifeq ($(CC_TYPE),msvc)
$(SHARED_LIB): $(LIBRARY_OBJECTS:%$(OBJ_EXT)=%_so$(OBJ_EXT))
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $^ $(SHARED_LIB_LDFLAGS) $(LDFLAGS) $(CFLAG_OUTPUT_EXE)'$@' || { rm -f '$@' '$(@:%.dll=%.lib)' '$(@:%.dll=%.exp)'; false; }
	$(Q)rm -f $(@:%.dll=%.exp)
else
$(SHARED_LIB): $(LIBRARY_OBJECTS:%$(OBJ_EXT)=%_so$(OBJ_EXT))
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $^ $(SHARED_LIB_LDFLAGS) $(LDFLAGS) $(CFLAG_OUTPUT_EXE)'$@'
endif

$(STATIC_LIB): $(LIBRARY_OBJECTS)
	$(ECHO) 'Archiving $@'
	$(Q)$(AR_rcu_)'$@' $^
	$(Q)$(RANLIB) '$@'

#--------------------------- Tool build rules ----------------------------#

ifneq ($(filter 1,$(BUILD_SHARED) $(BUILD_STATIC)),)

TOOL_SELFLIB = $(call if-true,BUILD_SHARED,$(SHARED_LIB),$(STATIC_LIB))
TOOL_SELFLIB_LINK = $(if $(filter msvc,$(CC_TYPE)),$(STATIC_LIB),$(TOOL_SELFLIB))
$(TOOL_BINS) : %$(EXE_EXT): tools/%$(OBJ_EXT) $(TOOL_SELFLIB)
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(filter-out $(TOOL_SELFLIB),$^) $(BASE_LDFLAGS) $(LDFLAGS) $(CFLAG_OUTPUT_EXE)'$@' $(TOOL_SELFLIB_LINK) $(TOOL_LIBS) $(LIBS)

else

$(TOOL_BINS):
	$(error Cannot build the tool programs without building the library)

endif

$(TOOL_BINS): BASE_CFLAGS += $(CFLAG_INCLUDE_DIR)include

nogg-benchmark$(EXE_EXT): TOOL_LIBS += $(or \
    $(shell pkg-config --libs vorbisfile ogg 2>/dev/null), \
    -lvorbisfile -lvorbis -logg)
tools/nogg-benchmark$(OBJ_EXT): BASE_CFLAGS += \
    $(shell pkg-config --cflags vorbisfile 2>/dev/null) \
    $(if $(TREMOR_SOURCE),-DHAVE_TREMOR -I'$(TREMOR_SOURCE)')

ifneq ($(TREMOR_SOURCE),)
nogg-benchmark$(EXE_EXT): tools/nogg-benchmark$(OBJ_EXT) $(TOOL_SELFLIB) \
    $(patsubst $(TREMOR_SOURCE)/%.c,tools/tremor-%$(OBJ_EXT),$(filter-out %_example.c,$(wildcard $(TREMOR_SOURCE)/*.c)))
tools/tremor-%$(OBJ_EXT): $(TREMOR_SOURCE)/%.c $(wildcard $(TREMOR_SOURCE)/*.h) tools/tremor-wrapper.h
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
tools/tremor-%$(OBJ_EXT): BASE_CFLAGS := \
    $(filter-out -W%,$(subst -std=c99,-std=gnu99,$(BASE_CFLAGS))) \
    $(CFLAG_INCLUDE_FILE)tools/tremor-wrapper.h
endif

#--------------------------- Test build rules ----------------------------#

$(TEST_BINS) : %$(EXE_EXT): %$(OBJ_EXT) $(STATIC_LIB)
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $^ $(BASE_LDFLAGS) $(LDFLAGS) $(CFLAG_OUTPUT_EXE)'$@' $(LIBS)

tests/coverage$(EXE_EXT): tests/coverage-main$(OBJ_EXT) $(LIBRARY_OBJECTS:%$(OBJ_EXT)=%_cov$(OBJ_EXT)) $(TEST_SOURCES:%.c=%_cov$(OBJ_EXT))
	$(ECHO) 'Linking $@'
	$(Q)$(CC) $(ALL_CFLAGS) $^ $(BASE_LDFLAGS) $(LDFLAGS) $(CFLAG_OUTPUT_EXE)'$@' $(LIBS) --coverage

tests/coverage-main$(OBJ_EXT): tests/coverage-tests.h

tests/coverage-tests.h: $(TEST_SOURCES)
	$(ECHO) 'Generating $@'
	$(Q)( \
	    for file in $(TEST_SOURCES:%.c=%); do \
	        file_mangled=_`echo "$${file}" | sed -e 's/[^A-Za-z0-9]/_/g'`; \
	        echo "TEST($${file_mangled})"; \
	    done \
	) >'$@'

#----------------------- Common compilation rules ------------------------#

%$(OBJ_EXT): %.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) $(call CC-autodependency-flags,$(@:%$(OBJ_EXT)=%.d.tmp)) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
	$(call filter-deps,$@,$(@:%$(OBJ_EXT)=%.d))

# We generate separate dependency files for shared objects even though the
# contents are the same as for static objects to avoid parallel builds
# colliding when writing the dependencies.
%_so$(OBJ_EXT): BASE_CFLAGS += $(SHARED_LIB_CFLAGS)
%_so$(OBJ_EXT): %.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) $(call CC-autodependency-flags,$(@:%$(OBJ_EXT)=%.d.tmp)) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
	$(call filter-deps,$@,$(@:%$(OBJ_EXT)=%.d))

src/%_cov$(OBJ_EXT): BASE_CFLAGS += $(CFLAG_NOOPT)
src/%_cov$(OBJ_EXT): src/%.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)$(CC) $(ALL_CFLAGS) --coverage $(call CC_autodependency_flag,$(@:%$(OBJ_EXT)=%.d.tmp)) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
	$(call filter-deps,$@,$(@:%$(OBJ_EXT)=%.d))

tests/%$(OBJ_EXT): BASE_CFLAGS += $(TEST_CFLAGS)
tests/%_cov$(OBJ_EXT): tests/%.c
	$(ECHO) 'Compiling $< -> $@'
	$(Q)file_mangled=_`echo '$(<:%.c=%)' | sed -e 's/[^A-Za-z0-9]/_/g'`; \
	    $(CC) $(ALL_CFLAGS) "-Dmain=$${file_mangled}" \
	        $(if $(filter -Wmissing-declarations,$(ALL_CFLAGS)),-Wno-missing-declarations) \
	        $(call CC-autodependency-flags,$(@:%$(OBJ_EXT)=%.d.tmp)) $(CFLAG_OUTPUT)'$@' $(CFLAG_COMPILE) '$<'
	$(call filter-deps,$@,$(@:%$(OBJ_EXT)=%.d))

#-------------------- Autogenerated dependency magic ---------------------#

define filter-deps
$(Q)test \! -f '$2.tmp' && rm -f '$2~' || sed \
    -e ':1' \
    -e 's#\(\\ \|[^ /]\)\+/\.\./##' \
    -e 't1' \
    -e ':2' \
    -e 's#/\./#/#' \
    -e 't2' \
    -e 's#^\(\([^ 	]*[ 	]\)*\)$(subst .,\.,$(1:%$(OBJECT_EXT)=%.h))#\1#' \
    -e 's#$(subst .,\.,$1)#$1 $(1:%$(OBJECT_EXT)=%.h)#' \
    <'$2.tmp' >'$2~'
$(Q)rm -f '$2.tmp'
$(Q)test \! -f '$2~' && rm -f '$2' || mv '$2~' '$2'
endef

# Don't try to include dependency data if we're not actually building
# anything.  This is particularly important for "clean" and "spotless",
# since an out-of-date dependency file which referenced a nonexistent
# target (this can arise from switching versions in place using version
# control, for example) would otherwise block these targets from running
# and cleaning out that very dependency file!
ifneq ($(filter-out clean spotless,$(or $(MAKECMDGOALS),default)),)
include $(sort $(wildcard $(patsubst %$(OBJ_EXT),%.d,\
    $(LIBRARY_OBJECTS) \
    $(LIBRARY_OBJECTS:%$(OBJ_EXT)=%_so$(OBJ_EXT)) \
    $(LIBRARY_OBJECTS:%$(OBJ_EXT)=%_cov$(OBJ_EXT)) \
    $(TEST_OBJECTS) \
    $(TEST_OBJECTS:%$(OBJ_EXT)=%_cov$(OBJ_EXT)) \
)))
endif

###########################################################################
