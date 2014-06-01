#
# libnogg: a decoder library for Ogg Vorbis streams
# Copyright (c) 2014 Andrew Church <achurch@achurch.org>
#
# This software may be copied and redistributed under certain conditions;
# see the file "COPYING" in the source code distribution for details.
# NO WARRANTY is provided with this software.
#

###########################################################################
############################## Configuration ##############################
###########################################################################

# In addition to the options below, standard build environment variables
# (CC, CFLAGS, etc.) are also recognized.

#----------------------------- Build options -----------------------------#

# BUILD_FRONTEND:  If this variable is set to 1, the build process will
# also build the sample frontend (tools/nogg-decode.c), creating the
# executable "nogg-decode" in the top directory.  Note that at least one
# of BUILD_SHARED and BUILD_STATIC must be enabled for the frontend to be
# built.
#
# The default is 0 (the sample frontend will not be built).

BUILD_FRONTEND = 0


# BUILD_SHARED:  If this variable is set to 1, the build process will
# create a shared library (typically "libnogg.so" on Unix-like systems).
# If the variable is set to 0, no shared library will not be built.
#
# The default is 1 (a shared library will be built).

BUILD_SHARED = 1


# BUILD_STATIC:  If this variable is set to 1, the build process will
# create a static library (typically "libnogg.a" on Unix-like systems).
# If the variable is set to 0, no static library will not be built.
#
# It is possible, though mostly meaningless, to set both BUILD_SHARED and
# BUILD_STATIC to 0.  In this case, "make" will do nothing, and "make
# install" will install only the library header file.
#
# The default is 1 (a static library will be built).

BUILD_STATIC = 1


# INSTALL_PKGCONFIG:  If this variable is set to 1, the build process will
# install a control file for the "pkg-config" tool as
# "$(LIBDIR)/pkgconfig/nogg.pc".
#
# The default is 0 (nogg.pc will not be installed).

INSTALL_PKGCONFIG = 0


# USE_STDIO:  If this variable is set to 1, the library will include
# support for reading files from the filesystem using C stdio.  If the
# variable is set to 0, this support will be disabled, and the library
# will not reference any stdio functions.
#
# The default is 1 (stdio support enabled).

USE_STDIO = 1


# WARNINGS_AS_ERRORS:  If this variable is set to 1, the build will abort
# if the compiler emits any warnings.
#
# The default is 0 (warnings will not abort the build).

WARNINGS_AS_ERRORS = 0

#----------------------- Installation target paths -----------------------#

# BINDIR:  Sets the directory into which the sample frontend (nogg-decode)
# will be installed.  This path is not used if BUILD_FRONTEND is set to 0.
#
# The default is "$(PREFIX)/bin".

BINDIR = $(PREFIX)/bin


# DESTDIR:  Sets the location of the installation root directory.  This
# string, if any, is prefixed to all target pathnames during installation,
# but it is not included in pathnames
# allowing files to be installed 
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
VERSION = 0.1

# Output filenames:
FRONTEND_BIN = $(PACKAGE)-decode
SHARED_LIB = lib$(PACKAGE).so
STATIC_LIB = lib$(PACKAGE).a
TEST_BIN = $(PACKAGE)-test

# Library object filenames:
LIBRARY_OBJECTS := $(sort $(strip \
    $(patsubst %.c,%.o,$(wildcard src/*/*.c))))

###########################################################################
############################ Helper functions #############################
###########################################################################

# if-true:  Return the second parameter if the variable named by the first
# parameter has the value 1, the third parameter (which may be omitted)
# otherwise.

if-true = $(if $(filter 1,$($1)),$2,$3)


# define-if-true:  Return an appropriate -D compiler option for the given
# variable name if its value is 1, the empty string otherwise.

define-if-true = $(call if-true,$1,-D$1)

###########################################################################
############################# Toolchain setup #############################
###########################################################################

# Default tool program names:

CC ?= cc
AR ?= ar
RANLIB ?= ranlib


# Try and guess what sort of compiler we're using, so we can set
# appropriate default options.

CC_TYPE = unknown
ifneq ($(filter clang%,$(subst -, ,$(CC))),)
    CC_TYPE = clang
else ifneq ($(filter icc%,$(subst -, ,$(CC))),)
    CC_TYPE = icc
else ifneq ($(filter gcc%,$(subst -, ,$(CC))),)
    CC_TYPE = gcc
else
    CC_VERSION_TEXT := $(shell "$(CC)" --version 2>&1)
    ifneq (,$(filter clang LLVM,$(CC_VERSION_TEXT)))
        CC_TYPE = clang
    else ifneq (,$(filter gcc,$(CC_VERSION_TEXT)))
        CC_TYPE = gcc
    endif
endif

ifeq ($(CC_TYPE),clang)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wpointer-arith -Wshadow -Wwrite-strings \
        -Wundef -Wno-unused-parameter -Wvla
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else ifeq ($(CC_TYPE),gcc)
    BASE_FLAGS = -O2 -pipe -g -I. \
        -Wall -Wextra $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wcast-align -Winit-self -Wlogical-op -Wpointer-arith -Wshadow \
        -Wwrite-strings -Wundef -Wno-unused-parameter -Wvla
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else ifeq ($(CC_TYPE),icc)
    BASE_FLAGS = -O2 -g -I. \
        $(call if-true,WARNINGS_AS_ERRORS,-Werror) \
        -Wpointer-arith -Wreturn-type -Wshadow -Wuninitialized \
        -Wunknown-pragmas -Wunused-function -Wunused-variable -Wwrite-strings
    BASE_CFLAGS = $(BASE_FLAGS) -std=c99 \
        -Wmissing-declarations -Wstrict-prototypes
else
    $(warning *** Warning: Unknown compiler type.)
    $(warning *** Make sure your CFLAGS are set correctly!)
endif


# Final flag set.  Note that the user-specified $(CFLAGS) reference comes
# last so the user can override any of our default flags.

ALL_DEFS = $(strip \
    $(call define-if-true,USE_STDIO) \
    -DVERSION=\"$(VERSION)\")

ALL_CFLAGS = $(BASE_CFLAGS) $(LIBRARY_INCDIRS) $(ALL_DEFS) $(CFLAGS)

###########################################################################
############################### Build rules ###############################
###########################################################################

#----------------------------- Entry points ------------------------------#

.PHONY: all all-frontend all-shared all-static install install-frontend
.PHONY: install-headers install-pc install-shared install-static
.PHONY: test clean spotless


all: $(call if-true,BUILD_SHARED,all-shared) \
     $(call if-true,BUILD_STATIC,all-static) \
     $(call if-true,BUILD_FRONTEND,all-frontend)

all-frontend: $(FRONTEND_BIN)

all-shared: $(SHARED_LIB)

all-static: $(STATIC_LIB)


install: $(call if-true,BUILD_SHARED,install-shared) \
         $(call if-true,BUILD_STATIC,install-static) \
         install-headers \
         $(call if-true,INSTALL_PKGCONFIG,install-pc) \
         $(call if-true,BUILD_FRONTEND,install-frontend)

install-frontend: all-frontend
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -pf $(FRONTEND_BIN) "$(DESTDIR)$(BINDIR)/"

install-headers:
	mkdir -p "$(DESTDIR)$(INCDIR)"
	cp -pf include/nogg.h "$(DESTDIR)$(INCDIR)/"

install-pc:
	mkdir -p "$(DESTDIR)$(LIBDIR)/pkgconfig"
	sed \
	    -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@INCDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(INCDIR))|g' \
	    -e 's|@LIBDIR@|$(patsubst $(PREFIX)%,$${prefix}%,$(LIBDIR))|g' \
	    -e 's|@VERSION@|$(VERSION)|g'\
	    <$(PACKAGE).pc.in >"$(DESTDIR)$(LIBDIR)/pkgconfig/$(PACKAGE).pc"

install-shared: all-shared
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	cp -pf $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/$(SHARED_LIB).$(VERSION)"
	ln -s $(SHARED_LIB).$(VERSION) "$(DESTDIR)$(LIBDIR)/$(SHARED_LIB).$(firstword $(subst ., ,$(VERSION)))"
	ln -s $(SHARED_LIB).$(VERSION) "$(DESTDIR)$(LIBDIR)/$(SHARED_LIB)"

install-static: all-static
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	cp -pf $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/"


test: $(TEST_BIN)
	./$(TEST_BIN)


clean:
	rm -f src/*/*.o test/*.o tools/*.o

spotless: clean
	rm -f $(SHARED_LIB) $(STATIC_LIB) $(FRONTEND_BIN) $(TEST_BIN)

#-------------------------- Library build rules --------------------------#

$(SHARED_LIB): $(LIBRARY_OBJECTS:%.o=%_so.o)
	$(CC) \
	    -shared \
	    -Wl,-soname=lib$(PACKAGE).so.$(firstword $(subst ., ,$(VERSION))) \
	    -o $@ $^

$(STATIC_LIB): $(LIBRARY_OBJECTS)
	$(AR) rcu $@ $^
	$(RANLIB) $@

#------------------------- Frontend build rules --------------------------#

ifneq ($(filter 1,$(BUILD_SHARED) $(BUILD_STATIC)),)

$(FRONTEND_BIN): tools/nogg-decode.o $(call if-true,BUILD_SHARED,$(SHARED_LIB),$(STATIC_LIB))
	$(CC) $(LDFLAGS) -o $@ $^ -lm

tools/nogg-decode.o: tools/nogg-decode.c
	$(CC) $(ALL_CFLAGS) -Iinclude -o $@ -c $<

else

$(FRONTEND_BIN):
	$(error Cannot build the frontend without building the library)

endif

#--------------------------- Test build rules ----------------------------#

$(TEST_BIN): $(LIBRARY_OBJECTS) $(patsubst %.c,%.o,$(wildcard test/*.c))
	$(CC) $(LDFLAGS) -o $@ $^

$(patsubst %.c,%.o,$(wildcard test/*.c)): $(wildcard test/*.h)

#----------------------- Common compilation rules ------------------------#

%.o: %.c $(wildcard include/*.h)
	$(CC) $(ALL_CFLAGS) -o $@ -c $<

%_so.o: %.c $(wildcard include/*.h)
	$(CC) $(ALL_CFLAGS) -fPIC -o $@ -c $<

###########################################################################
