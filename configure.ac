dnl -*- mode: m4 -*-
AC_PREREQ([2.69])

dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([calendar_major_version],[3])
m4_define([calendar_minor_version],[25])
m4_define([calendar_micro_version],[0])
m4_define([calendar_version],[calendar_major_version.calendar_minor_version.calendar_micro_version])
m4_define([calendar_interface_age],[0])
m4_define([calendar_bugreport_url],
	  [https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-calendar])
m4_define([calendar_debug_default],
          [m4_if(m4_eval(calendar_minor_version % 2), [1], [yes], [minimum])])


dnl ***********************************************************************
dnl Initialize Autoconf
dnl ***********************************************************************
AC_INIT([gnome-calendar],
        [calendar_version],
        [calendar_bugreport_url],
        [gnome-calendar],
        [https://wiki.gnome.org/Apps/Calendar])

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

dnl ***********************************************************************
dnl Setup Debug and Tracing Support
dnl ***********************************************************************
AC_ARG_ENABLE(tracing,
              AS_HELP_STRING([--enable-tracing=@<:@no/yes@:>@],
                             [add extra debugging information @<:@default=no@:>@]),
              ,
              enable_tracing=no)
AS_IF([test "x$enable_tracing" = "xyes"],[enable_debug=yes ENABLE_TRACING=1],[ENABLE_TRACING=0])
AC_SUBST(ENABLE_TRACING)

AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=calendar_debug_default@:>@]),
              ,
              enable_debug=calendar_debug_default)
AS_CASE(["$enable_debug"],
        [yes],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -O0"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -g"
        ],
        [minimum],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [no],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_ASSERT"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CHECKS"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [])
AC_SUBST(DEBUG_CFLAGS)

BUGREPORT_URL=calendar_bugreport_url
AC_SUBST(BUGREPORT_URL)

dnl ================================================================
dnl Misc
dnl ================================================================
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])

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
      src/gcal-debug.h
      data/Makefile
      data/org.gnome.Calendar.desktop.in
      data/org.gnome.Calendar.search-provider.ini.in
      data/icons/Makefile
      po/Makefile.in
])

AC_OUTPUT

dnl ==========================================================================
echo "

    GNOME Calendar $VERSION
    =========================

        Source ...........................  ${srcdir}
        Prefix ...........................  ${prefix}
        Compiler .........................  ${CC}

    Development options
        Enable Debug .....................  ${enable_debug}
        Enable Tracing ...................  ${enable_tracing}

    Now type 'make' to build $PACKAGE
"
