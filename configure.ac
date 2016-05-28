dnl -*- mode: m4 -*-
AC_PREREQ([2.69])
AC_INIT([gnome-calendar],[3.22.1],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-calendar],[gnome-calendar],[https://wiki.gnome.org/Apps/Calendar])

AX_IS_RELEASE([minor-version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_SILENT_RULES([yes])

# require automake 1.13
AM_INIT_AUTOMAKE([1.13 dist-xz no-dist-gzip tar-ustar subdir-objects])
AM_MAINTAINER_MODE([enable])

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.40.6])
PKG_PROG_PKG_CONFIG([0.22])

GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])

AS_IF([test "x$USE_NLS" != "xno"],
      [AC_DEFINE([ENABLE_NLS], [1], [Native language support is enabled])])

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
AC_MSG_CHECKING([for _NL_TIME_FIRST_WEEKDAY])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                   [[nl_langinfo(_NL_TIME_FIRST_WEEKDAY);]])],
               [langinfo_ok=yes], [langinfo_ok=no])
AC_MSG_RESULT([$langinfo_ok])
if test "$langinfo_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_TIME_FIRST_WEEKDAY], [1],
            [Define if _NL_TIME_FIRST_WEEKDAY is available])
fi

## don't rerun to this point if we abort
AC_CACHE_SAVE

AC_PROG_INSTALL
AC_SEARCH_LIBS([strerror],[cposix])
AC_HEADER_STDC

# enable libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
LT_LANG([C])

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
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])

AX_COMPILER_FLAGS

GLIB_MIN_VERSION=2.43.4
GTK_MIN_VERSION=3.21.6
ECAL_REQUIRED=3.13.90
EDATASERVER_REQUIRED=3.17.1
EDATASERVERUI_REQUIRED=3.17.1
LIBICAL_REQUIRED=1.0
GOA_MIN_VERSION=3.2.0
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=3.21.2

PKG_CHECK_MODULES([CALENDAR], [
                  gmodule-export-2.0
                  gio-2.0 >= $GLIB_MIN_VERSION
                  glib-2.0 >= $GLIB_MIN_VERSION
                  goa-1.0 >= $GOA_MIN_VERSION
                  gtk+-3.0 >= $GTK_MIN_VERSION
                  libecal-1.2 >= $ECAL_REQUIRED
                  libedataserver-1.2 >= $EDATASERVER_REQUIRED
                  libedataserverui-1.2 >= $EDATASERVERUI_REQUIRED
                  libical >= $LIBICAL_REQUIRED
                  gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED])

AC_CHECK_LIB([ical], [icaltime_days_in_year], [],
                [AC_MSG_ERROR([Error: icaltime_days_in_year() not found in libical!. Upgrade your libical library.])])

AX_REQUIRE_DEFINED([APPSTREAM_XML])
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
        flags:     ${WARN_CFLAGS} ${WARN_LDFLAGS}
        flags:     ${CALENDAR_CFLAGS} ${CALENDAR_LIBS} ${LIBS}

        Now type 'make' to build $PACKAGE
"
