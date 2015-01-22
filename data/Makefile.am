SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.Calendar.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = appdata/org.gnome.Calendar.appdata.xml.in

@INTLTOOL_DESKTOP_RULE@

servicedir = $(datadir)/dbus-1/services
service_DATA = org.gnome.Calendar.service

%.service: %.service.in Makefile
	$(AM_V_GEN) [ -d $(@D) ] || $(mkdir_p) $(@D) ; \
	            sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@.tmp && mv $@.tmp $@

org.gnome.Calendar.service:
	$(AM_V_GEN) (echo '[D-BUS Service]'; \
	            echo 'Name=org.gnome.Calendar'; \
	            echo 'Exec=${bindir}/gnome-calendar --gapplication-service') > $@.tmp && \
	            mv $@.tmp $@

@GSETTINGS_RULES@

gsettings_ENUM_NAMESPACE = org.gnome.calendar
gsettings_ENUM_FILES = \
	$(top_srcdir)/src/gcal-utils.h

gsettingsschema_in_files = org.gnome.calendar.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST=                     \
  appdata/org.gnome.Calendar.appdata.xml \
  org.gnome.Calendar.desktop    \
  calendar.gresource.xml        \
  ui/date-selector.ui           \
  ui/edit-dialog.ui             \
  ui/menus.ui                   \
  ui/search-view.ui             \
  ui/time-selector.ui           \
  ui/window.ui                  \
  ui/year-view.ui               \
  theme/gtk-styles.css          \
  $(appdata_in_files)           \
  $(desktop_in_files)           \
  $(gsettingsschema_in_files)

CLEANFILES =                    \
  $(service_DATA)               \
  $(desktop_DATA)               \
  $(gsettings_SCHEMAS)          \
  $(appdata_DATA)
