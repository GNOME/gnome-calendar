calendar_incs +=  include_directories('.')

blueprints = custom_target(
  'blueprints',
  input: files(
    'gcal-agenda-view.blp',
    'gcal-month-cell.blp',
    'gcal-month-popover.blp',
    'gcal-month-view.blp',
    'gcal-week-header.blp',
    'gcal-week-view.blp',
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
  'views-resources',
  'views.gresource.xml',
  c_name: 'views',
  dependencies: blueprints,
)

sources += files(
  'gcal-agenda-view.c',
  'gcal-month-cell.c',
  'gcal-month-popover.c',
  'gcal-month-view.c',
  'gcal-month-view-row.c',
  'gcal-view.c',
  'gcal-week-grid.c',
  'gcal-week-header.c',
  'gcal-week-hour-bar.c',
  'gcal-week-view.c',
  'gcal-week-view-common.c',
)
