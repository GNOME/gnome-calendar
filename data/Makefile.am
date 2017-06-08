SUBDIRS = icons

################
# Desktop files
################

desktopdir = $(datadir)/applications
desktop_in_files = org.gnome.Calendar.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@


##############################
# GNOME Shell search provider
##############################

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = org.gnome.Calendar.search-provider.ini


################
# Appdata files
################

appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = $(srcdir)/appdata/org.gnome.Calendar.appdata.xml.in

%.xml: %.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@


###################
# GSettings schema
###################

gsettings_ENUM_NAMESPACE = org.gnome.calendar
gsettings_ENUM_FILES = \
	$(top_srcdir)/src/gcal-utils.h

gsettings_SCHEMAS = org.gnome.calendar.gschema.xml

.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@


#####################
# DBus service files
#####################

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

EXTRA_DIST=                     \
  appdata/org.gnome.Calendar.appdata.xml \
  calendar.gresource.xml        \
  shell-search-provider-dbus-interfaces.xml \
  ui/alarm-row.ui               \
  ui/calendar-row.ui            \
  ui/date-chooser.ui            \
  ui/date-selector.ui           \
  ui/edit-dialog.ui             \
  ui/help-overlay.ui            \
  ui/menus.ui                   \
  ui/multi-choice.ui            \
  ui/quick-add-popover.ui       \
  ui/search-view.ui             \
  ui/sidebar.ui                 \
  ui/source-dialog.ui           \
  ui/time-selector.ui           \
  ui/week-header.ui             \
  ui/week-view.ui               \
  ui/window.ui                  \
  ui/year-view.ui               \
  theme/gtk-styles.css          \
  $(appdata_in_files)           \
  $(desktop_in_files)           \
  $(gsettings_SCHEMAS)          \
  $(searchprovider_DATA)

CLEANFILES =                    \
  $(appdata_DATA)               \
  $(service_DATA)               \
  $(desktop_DATA)
