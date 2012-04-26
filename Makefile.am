ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = src po doc

gnome_calendardocdir = ${prefix}/doc/gnome_calendar
gnome_calendardoc_DATA = \
	README\
	INSTALL\
	NEWS


INTLTOOL_FILES = intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST =                  \
    $(gnome_calendardoc_DATA) \
	$(INTLTOOL_FILES)         \
	THANKS					  \
    TODO                      \
    autogen.sh


DISTCLEANFILES = intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache


# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(gnome_calendardocdir)
