subdir('calendar-management')
subdir('gtk')
subdir('views')

calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'gui-resources',
  'gui.gresource.xml',
  c_name: 'gui',
)

sources += files(
  'gcal-alarm-row.c',
  'gcal-application.c',
  'gcal-calendar-popover.c',
  'gcal-date-chooser.c',
  'gcal-date-chooser-day.c',
  'gcal-date-selector.c',
  'gcal-edit-dialog.c',
  'gcal-event-widget.c',
  'gcal-multi-choice.c',
  'gcal-quick-add-popover.c',
  'gcal-search-button.c',
  'gcal-time-selector.c',
  'gcal-weather-settings.c',
  'gcal-window.c',
)
