AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\" \
	$(GNOME_CALENDAR_CFLAGS)

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = gnome_calendar

gnome_calendar_SOURCES = \
	main.c \
	gcal-application.h \
	gcal-application.c	

gnome_calendar_LDFLAGS = \
	-Wl,--export-dynamic

gnome_calendar_LDADD = $(GNOME_CALENDAR_LIBS)


