#
# $Id$
#
# Copyright (C) 2004 Bruce Bennett <bruts@adelphia.net>
# Portions Copyright (C) 2004-2008  The Xastir Group
# (Modified for the Xastir project under GPL license)
#
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(db2APRS, 0.2.0, bruts@adelphia.net)
AM_INIT_AUTOMAKE(db2APRS, 0.2.0)
AC_CONFIG_SRCDIR([src/db2APRS.c])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC

# Checks for libraries.
# Use MySQL's script for library config, if available
MYSQL_CONFIG=mysql_config
AC_MSG_CHECKING(for a fully installed MySQL)
if ${MYSQL_CONFIG} --libs > /dev/null 2>&1
then
    MYSQL_VERSION=`${MYSQL_CONFIG} --version`
    MYSQL_LIBS=`${MYSQL_CONFIG} --libs`
    LIBS=`${MYSQL_CONFIG} --libs`
    MYSQL_CFLAGS=`${MYSQL_CONFIG} --cflags`
    AC_SUBST(MYSQL_LIBS)
    AC_SUBST(MYSQL_CFLAGS)
    AC_MSG_RESULT(...found ${MYSQL_VERSION})
else
    AC_MSG_RESULT(MySQL is not fully installed)
    AC_MSG_CHECKING(if there at least are the needed MySQL client libs)
    AC_CHECK_LIB(mysqlclient,mysql_close)
    if test $ac_cv_lib_mysqlclient_mysql_close = no; then
    
        AC_MSG_ERROR(*** No MySQL client library found - See README ***)
    else
        AC_MSG_RESULT(found a usable libmysqlclient)
    fi
fi
# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h syslog.h unistd.h])
if ${MYSQL_CONFIG} --libs > /dev/null 2>&1
then
    AC_MSG_RESULT(mysql.h path defined in CFLAGS)
else
    AC_CHECK_HEADERS([mysql.h],MYSQL_INC="yes",AC_MSG_ERROR(*** MySQL include file mysql.h not found - See README ***))
fi
# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset select socket strrchr strtol])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
