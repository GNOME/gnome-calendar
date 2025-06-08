calendar_incs +=  include_directories('.')

blueprints = custom_target(
  'blueprints',
  input: files(
    'gcal-import-dialog.blp',
    'gcal-import-file-row.blp',
  ),
  output: '.',
  command: [
    find_program('blueprint-compiler'),
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)

built_sources += gnome.compile_resources(
  'importer-resources',
  'importer.gresource.xml',
  c_name: 'importer',
  dependencies: blueprints,
)

sources += files(
  'gcal-import-dialog.c',
  'gcal-import-file-row.c',
  'gcal-importer.c',
)
