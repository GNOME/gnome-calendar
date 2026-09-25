calendar_incs +=  include_directories('.')

blueprints = custom_target(
 'blueprints',
  input: files(
    'calendar-row.blp',
    'gcal-calendar-management-dialog.blp',
    'gcal-calendars-page.blp',
    'gcal-edit-calendar-page.blp',
    'gcal-new-local-calendar-page.blp',
    'gcal-new-online-calendar-page.blp',
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
  'calendar-management-resources',
  'calendar-management.gresource.xml',
  c_name: 'calendar_management',
  dependencies: blueprints,
)

sources += files(
  'gcal-calendar-management-dialog.c',
  'gcal-calendar-management-page.c',
  'gcal-calendars-page.c',
  'gcal-edit-calendar-page.c',
  'gcal-new-local-calendar-page.c',
  'gcal-new-online-calendar-page.c',
)
