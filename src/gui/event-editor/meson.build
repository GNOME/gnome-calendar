calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'event-editor-resources',
  'event-editor.gresource.xml',
  c_name: 'event_editor',
)

sources += files(
  'gcal-alarm-row.c',
  'gcal-date-chooser.c',
  'gcal-date-chooser-day.c',
  'gcal-date-selector.c',
  'gcal-edit-dialog.c',
  'gcal-multi-choice.c',
  'gcal-time-selector.c',
)
