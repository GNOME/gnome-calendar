dnl -*- mode: m4 -*-
AC_PREREQ([2.69])
AC_INIT([gnome-calendar],[3.16.1],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-calendar],[gnome-calendar],[https://wiki.gnome.org/Apps/Calendar])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_SILENT_RULES([yes])

# require automake 1.11.1
AM_INIT_AUTOMAKE([1.11.1 dist-xz no-dist-gzip tar-ustar])
AM_MAINTAINER_MODE([enable])

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.40.6])
PKG_PROG_PKG_CONFIG([0.22])

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT

## don't rerun to this point if we abort
AC_CACHE_SAVE

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_INSTALL
AC_SEARCH_LIBS([strerror],[cposix])
AC_HEADER_STDC

# no stupid static libraries
AC_DISABLE_STATIC([])

# enable libtool
LT_PREREQ([2.2.6])
LT_INIT

## don't rerun to this point if we abort
AC_CACHE_SAVE

## checks
AC_CHECK_HEADERS([langinfo.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([string.h])

AC_CHECK_FUNCS([floor])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([nl_langinfo])
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([setlocale])
AC_CHECK_FUNCS([strstr])

GLIB_GSETTINGS

dnl ================================================================
dnl Misc
dnl ================================================================
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

GNOME_COMPILE_WARNINGS(maximum)

MAINTAINER_COMPILER_FLAGS="\
  $MAINTAINER_COMPILER_FLAGS\
 -Wall -Wcast-align -Wuninitialized\
 -Wno-strict-aliasing -Wempty-body -Wformat\
 -Wformat-security -Wformat-nonliteral -Winit-self\
 -Wdeclaration-after-statement -Wvla\
 -Wpointer-arith -Wmissing-declarations\
 -Wcast-align -Wmissing-prototypes\
 -Wredundant-decls"

# strip leading spaces
MAINTAINER_COMPILER_FLAGS=${MAINTAINER_COMPILER_FLAGS#*  }
AC_SUBST(MAINTAINER_COMPILER_FLAGS)

GLIB_MIN_VERSION=2.43.4
GTK_MIN_VERSION=3.15.4
ECAL_REQUIRED=3.13.90
EDATASERVER_REQUIRED=3.13.90
EDATASERVERUI_REQUIRED=3.13.90
LIBICAL_REQUIRED=0.43

PKG_CHECK_MODULES(CALENDAR,
                  gmodule-export-2.0
                  gio-2.0 >= $GLIB_MIN_VERSION
                  glib-2.0 >= $GLIB_MIN_VERSION
                  gtk+-3.0 >= $GTK_MIN_VERSION
                  libecal-1.2 >= $ECAL_REQUIRED
                  libedataserver-1.2 >= $EDATASERVER_REQUIRED
                  libedataserverui-1.2 >= $EDATASERVERUI_REQUIRED
                  libical >= $LIBICAL_REQUIRED)

APPSTREAM_XML

AC_CONFIG_FILES([
      Makefile
      src/Makefile
      data/Makefile
      data/org.gnome.Calendar.desktop.in
      data/org.gnome.Calendar.search-provider.ini.in
      data/icons/Makefile
      po/Makefile.in
])

AC_OUTPUT

dnl ==========================================================================
echo "

        gnome-calendar $VERSION
        ==================

        source at: ${srcdir}
        prefix:    ${prefix}
        compiler:  ${CC}
	flags:     ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}
	flags:     ${MAINTAINER_COMPILER_FLAGS}
	flags:     ${CALENDAR_CFLAGS} ${CALENDAR_LIBS} ${LIBS}

        Now type 'make' to build $PACKAGE
"
