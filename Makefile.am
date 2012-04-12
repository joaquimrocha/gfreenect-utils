## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src

gfreenectutilsdocdir = ${prefix}/doc/gfreenect-utils
gfreenectutilsdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS

EXTRA_DIST = $(gfreenectutilsdoc_DATA)

# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(gfreenectutilsdir)
