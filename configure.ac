#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT([blts-sqlite-perf], [0.0.1])
AM_INIT_AUTOMAKE([-Wall gnu])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([bltscommon], [blts_cli_main])
AC_CHECK_LIB([sqlite3], [sqlite3_open])

# Library configs
PKG_CHECK_MODULES(PACKAGE, [
        bltscommon
        sqlite3
])

AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([strerror])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
