## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

AM_CFLAGS = \
	$(GFREENECT_CFLAGS) \
	$(MAIN_DEPS_CFLAGS) \
	-I$(top_srcdir)/@PRJ_NAME@
	-Wall \
	-g

bin_PROGRAMS = record-depth-file depth-file-viewer

record_depth_file_SOURCES = \
	take-shot.c

record_depth_file_LDADD = \
	$(GFREENECT_LIBS) \
	$(MAIN_DEPS_LIBS)

depth_file_viewer_SOURCES = \
	depth-file-viewer.c

depth_file_viewer_LDADD = \
	$(MAIN_DEPS_LIBS) -lm
