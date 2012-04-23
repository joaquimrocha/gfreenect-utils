dnl Process this file with autoconf to produce a configure script.
dnl Created by Anjuta application wizard.

m4_define([prj_name],       [GFreenect Utils])
m4_define([prj_short_name], [gfreenectutils])
m4_define([prj_home],       [http://labs.igalia.com/gfreenect-utils])

# package version number (not shared library version)
# odd micro numbers indicate in-progress development
# even micro numbers indicate released versions
m4_define([prj_version_major], [0])
m4_define([prj_version_minor], [1])
m4_define([prj_version_micro], [1])

m4_define([prj_version],
          [prj_version_major.prj_version_minor.prj_version_micro])

AC_INIT([prj_name],
	[prj_version],
	[prj_home])

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

PRJ_NAME="prj_short_name"
AC_SUBST(PRJ_NAME)

AC_PROG_CC

CLUTTER_REQUIRED=1.8.4
GLIB_REQUIRED=2.28.0
PKG_CHECK_MODULES(MAIN_DEPS, clutter-1.0 >= CLUTTER_REQUIRED
                             glib-2.0 >= $GLIB_REQUIRED
                             gio-2.0 >= $GLIB_REQUIRED
                             gobject-2.0 >= $GLIB_REQUIRED)

GFREENECT_REQUIRED=0.1.4
PKG_CHECK_MODULES(GFREENECT, gfreenect-0.1 >= GFREENECT_REQUIRED)

AC_OUTPUT([
Makefile
src/Makefile
])
