dnl -*- mode: m4 -*-
AC_PREREQ(2.63)
AC_INIT([gnome-calendar],[0.0.2],[],[gnome-calendar])

AM_CONFIG_HEADER(config/config.h)
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# require automake 1.11
AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign])
AM_MAINTAINER_MODE([enable])

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.35.0])
PKG_PROG_PKG_CONFIG([0.22])

GETTEXT_PACKAGE=gnome_calendar
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT

## don't rerun to this point if we abort
AC_CACHE_SAVE

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_ISC_POSIX
AC_HEADER_STDC

# no stupid static libraries
AM_DISABLE_STATIC
# enable libtool
AC_PROG_LIBTOOL
# avoid libtool for LTCOMPILE, use it only to link

## don't rerun to this point if we abort
AC_CACHE_SAVE

LT_INIT

GLIB_GSETTINGS

GNOME_COMPILE_WARNINGS(maximum)
GNOME_COMMON_INIT
GNOME_MAINTAINER_MODE_DEFINES

MAINTAINER_COMPILER_FLAGS="$MAINTAINER_COMPILER_FLAGS                        \
                           -Wall -Wcast-align -Wuninitialized                \
                           -Wno-strict-aliasing -Wempty-body -Wformat        \
                           -Wformat-security -Wformat-nonliteral -Winit-self \
                           -Wdeclaration-after-statement -Wvla               \
                           -Wpointer-arith -Wmissing-declarations            \
                           -Wcast-align                                      \
                           -Wredundant-decls"

# strip leading spaces
MAINTAINER_COMPILER_FLAGS=${MAINTAINER_COMPILER_FLAGS#*  }
AC_SUBST(MAINTAINER_COMPILER_FLAGS)

GLIB_MIN_VERSION=2.30.0
GTK_MIN_VERSION=3.4.0
CLUTTER_MIN_VERSION=1.10
CLUTTER_GTK_MIN_VERSION=1.2
ECAL_REQUIRED=3.1.5
EDATASERVER_REQUIRED=3.1.5
EDATASERVERUI_REQUIRED=3.1.5

PKG_CHECK_MODULES(CALENDAR,
                  glib-2.0 >= $GLIB_MIN_VERSION
                  gtk+-3.0 >= $GTK_MIN_VERSION
                  clutter-1.0 >= CLUTTER_MIN_VERSION
                  clutter-gtk-1.0 >= CLUTTER_GTK_MIN_VERSION
                  libecal-1.2 >= $ECAL_REQUIRED
                  libedataserver-1.2 >= EDATASERVER_REQUIRED
                  libedataserverui-3.0 >= EDATASERVERUI_REQUIRED)

AC_CONFIG_FILES([
      Makefile
      src/Makefile
      po/Makefile.in
      doc/Makefile
])

AC_OUTPUT

dnl ==========================================================================
echo "

        gnome-calendar $VERSION
        ==================

        prefix:    ${prefix}
        compiler:  ${CC}

        Now type 'make' to build $PACKAGE
"
