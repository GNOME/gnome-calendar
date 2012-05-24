desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
  desktop_in_files = gnome-calendar.desktop.in

@INTLTOOL_DESKTOP_RULE@

gsettingsschema_in_files = org.gnome.calendar.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

styledir = $(pkgdatadir)/style

dist_style_DATA = \
  gtk-styles.css

EXTRA_DIST= \
  gnome-calendar.desktop \
  $(desktop_in_files) \
  $(gsettingsschema_in_files)

CLEANFILES = \
  $(desktop_DATA) \
  $(gsettings_SCHEMAS)
