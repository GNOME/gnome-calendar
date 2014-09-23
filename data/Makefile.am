desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
  desktop_in_files = gnome-calendar.desktop.in

@INTLTOOL_DESKTOP_RULE@

gsettings_ENUM_NAMESPACE = org.gnome.calendar
gsettings_ENUM_FILES = \
	$(top_srcdir)/src/gcal-utils.h

gsettingsschema_in_files = org.gnome.calendar.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST=                     \
  gnome-calendar.desktop        \
  calendar.gresource.xml        \
  ui/nav_bar.ui                 \
  ui/new-event.ui               \
  ui/viewport.ui                \
  theme/gtk-styles.css          \
  theme/close-window.svg        \
  $(desktop_in_files)           \
  $(gsettingsschema_in_files)

CLEANFILES =                    \
  $(desktop_DATA)               \
  $(gsettings_SCHEMAS)
