AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir) -I$(top_builddir)/src

EXTRA_PROGRAMS=
bin_PROGRAMS = turboxsltproc

turboxsltproc_SOURCES = turboxsltproc.c
turboxsltproc_LDFLAGS = 
turboxsltproc_DEPENDENCIES = $(DEPS)

DEPS = $(top_builddir)/src/libturboxsl.la

LDADDS = $(top_builddir)/src/libturboxsl.la

turboxsltproc_LDADD = $(LDADDS)

CLEANFILES = .memdump

$(top_builddir)/src/libturboxsl.la:
	cd $(top_builddir)/src && $(MAKE) libturboxsl.la

