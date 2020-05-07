#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mpi-p2p-checker], [0.1], [christoph.niethammer@hlrs.de])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_PROG_AR
LT_INIT([dlopen])
AC_CONFIG_SRCDIR([mpi-p2p-checker.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AX_PROG_CC_MPI(,,[
    AC_MSG_FAILURE([MPI compiler required.])
])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdlib.h string.h time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
