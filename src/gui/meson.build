subdir('calendar-management')
subdir('event-editor')
subdir('gtk')
subdir('icons')
subdir('importer')
subdir('views')

calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'gui-resources',
  'gui.gresource.xml',
  c_name: 'gui',
)

sources += files(
  'gcal-application.c',
  'gcal-calendar-button.c',
  'gcal-event-popover.c',
  'gcal-event-widget.c',
  'gcal-meeting-row.c',
  'gcal-overflow-bin.c',
  'gcal-overflow-bin-layout.c',
  'gcal-quick-add-popover.c',
  'gcal-search-button.c',
  'gcal-sync-indicator.c',
  'gcal-toolbar-end.c',
  'gcal-weather-settings.c',
  'gcal-window.c',
)
