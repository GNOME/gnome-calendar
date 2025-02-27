subdir('calendar-management')
subdir('common')
subdir('event-editor')
subdir('icons')
subdir('importer')
subdir('views')

calendar_incs +=  include_directories('.')

blueprints = custom_target(
  'blueprints',
  input: files(
    'gcal-calendar-list.blp',
    'gcal-calendar-navigation-button.blp',
    'gcal-event-popover.blp',
    'gcal-event-widget.blp',
    'gcal-meeting-row.blp',
    'gcal-quick-add-popover.blp',
    'gcal-search-button.blp',
    'gcal-sync-indicator.blp',
    'gcal-weather-settings.blp',
    'gcal-window.blp',
    'shortcuts-dialog.blp',
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
  'gui-resources',
  'gui.gresource.xml',
  c_name: 'gui',
  dependencies: blueprints,
)

sources += files(
  'gcal-application.c',
  'gcal-calendar-list.c',
  'gcal-calendar-navigation-button.c',
  'gcal-drop-overlay.c',
  'gcal-event-popover.c',
  'gcal-event-widget.c',
  'gcal-meeting-row.c',
  'gcal-overflow-bin.c',
  'gcal-overflow-bin-layout.c',
  'gcal-quick-add-popover.c',
  'gcal-search-button.c',
  'gcal-sync-indicator.c',
  'gcal-tests.c',
  'gcal-weather-settings.c',
  'gcal-window.c',
)
