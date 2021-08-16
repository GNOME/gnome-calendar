calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'importer-resources',
  'importer.gresource.xml',
  c_name: 'importer',
)

sources += files(
  'gcal-import-dialog.c',
  'gcal-import-file-row.c',
  'gcal-importer.c',
)
