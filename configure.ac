AC_INIT(gnome_calendar, 0.0.1)

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC

dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=gnome_calendar
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT

LT_INIT

PKG_CHECK_MODULES(GNOME_CALENDAR, 
                  [gio-2.0
                   glib-2.0
                   gtk+-3.0 >= 3.4
                   clutter-1.0
                   clutter-gtk-1.0
                   libedataserver-1.2
                   libedataserverui-3.0
                   libecal-1.2])

AC_OUTPUT([
  Makefile
  src/Makefile
  po/Makefile.in
])
