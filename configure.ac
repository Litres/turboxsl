dnl Process this file with autoconf to produce a configure script.

AC_INIT([xsltest],[1.0])
AC_CONFIG_SRCDIR([src/transform.c])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_CONFIG_MACRO_DIR([m4])

AC_ISC_POSIX
AC_PROG_CC_C99
AC_HEADER_STDC
LT_INIT

M_LIBS='-lpthread -lm -lmemcached -lck'
XSLT_LIBDIR='-L${libdir}'
XSLT_INCLUDEDIR='-I${includedir}'
XSLT_LIBS="-lturboxsl $LIBXML_LIBS $M_LIBS"
AC_SUBST(XSLT_LIBDIR)
AC_SUBST(XSLT_INCLUDEDIR)
AC_SUBST(XSLT_LIBS)

AC_SUBST(EXTRA_LIBS)
AC_SUBST(M_LIBS)

dnl These are for debug -O2 disabling 
CPPFLAGS="${CXXFLAGS}"
CXXFLAGS="${CPPFLAGS}"
CFLAGS=""
AC_SUBST(CXXFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(CLAGS)

AC_OUTPUT([
Makefile
libturboxsl.pc
src/Makefile
turboxsl-config
xsltproc/Makefile
tests/Makefile
tests/general/Makefile
tests/unsupported/Makefile
tests/docs/Makefile
])

