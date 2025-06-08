calendar_incs +=  include_directories('.')

blueprints = custom_target(
  'blueprints',
  input: files(
    'gcal-calendar-combo-row-item.blp',
    'gcal-calendar-combo-row.blp',
    'gcal-date-chooser-day.blp',
    'gcal-date-chooser.blp',
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
  'common-resources',
  'common.gresource.xml',
  c_name: 'common',
  dependencies: blueprints,
)

sources += files(
  'gcal-calendar-combo-row-item.c',
  'gcal-calendar-combo-row.c',
  'gcal-date-chooser.c',
  'gcal-date-chooser-day.c',
)
