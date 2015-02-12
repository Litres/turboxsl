## Process this file with automake to produce Makefile.in
#
#


DEPS = $(top_builddir)/src/libturboxsl.la

LDADDS = $(top_builddir)/src/libturboxsl.la

lib_LTLIBRARIES = libturboxsl.la

libturboxsl_la_SOURCES = \
  cache.c \
  debug.c \
  parse.c \
  output.c \
  templates.c \
  transform.c \
  threadpool.c \
  xpath.c \
  variables.c \
  functions.c \
  nodes.c \
  utils.c \
  strings.c \
  xmldict.c \
  md5.c \
  md5.h \
  md5_loc.h \
  rvalue.c \
  node_cache.c \
  xmldict.h \
  threadpool.h \
  node_cache.h \
  xslglobs.h \
  turboxsl.h \
  ltr_xsl.h

libturboxsl_la_LIBADD = -lpthread -lm

libturboxslincdir=$(includedir)/turboxsl
libturboxslinc_HEADERS = turboxsl.h

man_MANS = turboxsl.3

EXTRA_DIST = $(man_MANS)

