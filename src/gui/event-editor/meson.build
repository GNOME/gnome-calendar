calendar_incs +=  include_directories('.')

blueprints = custom_target(
  'blueprints',
  input: files(
    'gcal-alarm-row.blp',
    'gcal-date-chooser-row.blp',
    'gcal-date-selector.blp',
    'gcal-date-time-chooser.blp',
    'gcal-event-editor-dialog.blp',
    'gcal-multi-choice.blp',
    'gcal-notes-section.blp',
    'gcal-reminders-section.blp',
    'gcal-schedule-section.blp',
    'gcal-summary-section.blp',
    'gcal-time-chooser-row.blp',
    'gcal-time-zone-dialog-row.blp',
    'gcal-time-zone-dialog.blp',
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
  'event-editor-resources',
  'event-editor.gresource.xml',
  c_name: 'event_editor',
  dependencies: blueprints,
)

sources += files(
  'gcal-alarm-row.c',
  'gcal-date-chooser-row.c',
  'gcal-date-time-chooser.c',
  'gcal-date-selector.c',
  'gcal-event-schedule.c',
  'gcal-event-editor-dialog.c',
  'gcal-event-editor-section.c',
  'gcal-multi-choice.c',
  'gcal-notes-section.c',
  'gcal-reminders-section.c',
  'gcal-schedule-section.c',
  'gcal-summary-section.c',
  'gcal-time-zone-dialog.c',
  'gcal-time-zone-dialog-row.c',
)
