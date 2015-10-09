#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_INIT([libARMedia], [3.1.0], [ardrone@ardrone.org])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)

m4_include([../../ARSDKBuildUtils/Utils/m4/ARSDK.m4])

# automake initialisation and check for minimal automake API version 1.9
AM_INIT_AUTOMAKE

# use the C compiler for the following checks
AC_LANG([C])

##########################################################################
# Debug compilation support
##########################################################################
AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
    [enable debug data generation (def=no)])],
    [debugit="$enableval"],
    [debugit=no])
AC_MSG_RESULT([$debugit])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG],[],[Debug Mode])
    LOCAL_CFLAGS+=" -g -Wall -Werror -Wno-uninitialized -O0"
else
    AC_DEFINE([NDEBUG],[],[No-debug Mode])
    LOCAL_CFLAGS+=" -O3"
fi
AM_CONDITIONAL([DEBUG_MODE], [test "$debugit" = "yes"])

##########################################################################
# Disable VideoEncapsuler compilation
##########################################################################
AC_MSG_CHECKING([whether to build ARMEDIA_VideoEncapsuler.c])
AC_ARG_ENABLE([videoenc],
    [AS_HELP_STRING([--enable-videoenc],
    [enable video encapsuler (def=yes)])],
    [videoencit="$enableval"],
    [videoencit=yes])
AC_MSG_RESULT([$videoencit])

if test x"$videoencit" = x"yes"; then
    AC_DEFINE([AC_VIDEOENC],[],[VideoEncapsuler])
fi

##########################################################################
# Non versionned .so compilation support (for Android)
##########################################################################
AC_MSG_CHECKING([whether to put version number in .so files])
AC_ARG_ENABLE([so-version],
    [AS_HELP_STRING([--disable-so-version],
    [disable version names in .so files [default=enabled]])],
    [sover="$enableval"],
    [sover=yes])
AC_MSG_RESULT([$sover])

##########################################################################
# Define libraries directories
##########################################################################

##########################################################################
# Define includes directories
##########################################################################
# include local headers first
LOCAL_CFLAGS+=" -I$srcdir/../Includes -I$srcdir/../Sources"

CFLAGS+="$LOCAL_CFLAGS"
CPPFLAGS+="$LOCAL_CFLAGS"
OBJCFLAGS+="$LOCAL_CFLAGS"

# Add libARxxx deps
AR_DEPENDS([libARSAL], [Y], [libARSAL/ARSAL.h])
AR_DEPENDS([libARDiscovery], [Y], [libARDiscovery/ARDiscovery.h])
AR_DEPENDS([json], [Y], [json/json.h], [N])

##########################################################################
# Define binaries directories
##########################################################################

##########################################################################
# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_OBJC

AC_CHECK_PROGS([DOXYGEN], [doxygen])
m4_include(doxygen.ac)
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([libARMedia], [doxygen.cfg], [$srcdir/../Doc])
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

# Checks for header files.
AC_HEADER_STDC

##########################################################################
# Check headers
##########################################################################

# Check for UIKit.h
AC_LANG_PUSH([Objective C])
if test x$ac_cv_objc_compiler_gnu = xyes; then
    AC_CHECK_HEADERS([Foundation/Foundation.h], ,
        AC_MSG_WARN(The Foundation headers are required to use iOS OBJ-C features ! ))
    AC_CHECK_HEADERS([UIKit/UIKit.h], ,
        AC_MSG_WARN(The UIKit headers are required to use iOS OBJ-C features ! ),
        [#ifdef HAVE_FOUNDATION_FOUNDATION_H
        #include <Foundation/Foundation.h>
        #endif])
fi

AM_CONDITIONAL([HAVE_UIKIT], [test x"$ac_cv_header_UIKit_UIKit_h" = x"yes"])
AC_LANG_POP([Objective C])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Generate libtool version info number
if test x"$sover" = x"yes"; then
	LDFLAGS+=" -release $PACKAGE_VERSION"
else
    LDFLAGS+=" -avoid-version"
fi

# distribute additional compiler and linker flags
# --> set these variables instead of CFLAGS or LDFLAGS
AC_SUBST([CFLAGS])
AC_SUBST([LDFLAGS])
AC_SUBST([OBJCFLAGS])

# Checks for library functions.


# Generates Makefile
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix.........: $prefix
  Debug Build....: $debugit
  Objc Compiler..: $ac_cv_objc_compiler_gnu
  C Compiler.....: $CC $CFLAGS
  Linker.........: $LD $LDFLAGS $LIBS
  Doxygen........: ${DOXYGEN:-NONE}
  VideoEncapsuler: $videoencit
"
