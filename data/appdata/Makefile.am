@APPSTREAM_XML_RULES@
appstream_in_files = org.gnome.Calendar.appdata.xml.in
appstream_XML = $(appstream_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@
EXTRA_DIST = $(appdata_in_files)
CLEANFILES = $(appstream_XML)

-include $(top_srcdir)/git.mk
