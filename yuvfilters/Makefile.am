EXTRA_DIST = install_symlink.sh README.2-3pulldown .cvsignore
MAINTAINERCLEANFILES = Makefile.in

YUVFILTER =\
 libyuvthru.la\
 #

# for buggy version of libtools, link $(YSTMEDIA) if not USE_DLFCN_H

if USE_MEDIA_FILE
YSTFILE = $(top_builddir)/mjpegtools/libystfile.la
else
YSTFILE =
endif

if USE_MEDIA_STACK
YSTSTACK = $(top_builddir)/mjpegtools/libyststack.la
else
YSTSTACK =
endif

if USE_MEDIA_GST
YSTGST = $(top_builddir)/mjpegtools/libystgst.la
else
YSTGST =
endif

YSTMEDIA = $(YSTFILE) $(YSTSTACK) $(YSTGST)

if USE_DLFCN_H
YSTLIBRARIES = $(top_builddir)/mjpegtools/libyst.la
else
YSTLIBRARIES = $(YUVFILTER) $(top_builddir)/mjpegtools/libyst.la $(YSTMEDIA)
endif

LIBLDFLAGS = -release $(VERSION)

bin_PROGRAMS = yuvycsnoise yuvkineco yuvfilter
noinst_LIBRARIES = libyuvfilters.a
pkglib_LTLIBRARIES = $(YUVFILTER)

AM_CPPFLAGS = -I$(top_srcdir)/utils -I$(top_srcdir)/mjpegtools \
	-DPKGLIBDIR=\"$(pkglibdir)\" -DUSE_LIBYST=1
MJPEGLIB = $(top_builddir)/utils/libmjpegutils.a

yuvfilter_SOURCES = \
	yuvfilter.c yuvfilter_static.c yuvfilter_static.h
yuvfilter_LDADD = $(YSTLIBRARIES) $(MJPEGLIB) @LIBGETOPT_LIB@ -ldl
yuvfilter_LDFLAGS = -export-dynamic

libyuvthru_la_SOURCES = yuvthru.c
libyuvthru_la_LDFLAGS = $(LIBLDFLAGS)

libyuvfilters_a_SOURCES = \
	yuvfilters.h \
	addtask.c alloctask.c initframe.c putframe.c yuvstdin.c yuvstdout.c \
	yuvycsnoise.c yuvkineco.c

yuvkineco_SOURCES = main.c
yuvkineco_CFLAGS = -DFILTER=yuvkineco
yuvkineco_LDADD = libyuvfilters.a $(MJPEGLIB)

yuvycsnoise_SOURCES = main.c
yuvycsnoise_CFLAGS = -DFILTER=yuvycsnoise
yuvycsnoise_LDADD = libyuvfilters.a $(MJPEGLIB)

SYMLINKS	= $(YUVFILTER:lib%.la=%)
bin_SCRIPTS	= $(SYMLINKS)
INSTALL_SCRIPT	= sh $(srcdir)/install_symlink.sh
CLEANFILES	= $(SYMLINKS)

$(SYMLINKS): yuvfilter ; $(LN_S) yuvfilter $@
