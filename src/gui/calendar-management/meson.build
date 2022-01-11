calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'calendar-management-resources',
  'calendar-management.gresource.xml',
  c_name: 'calendar_management',
)

sources += files(
  'gcal-calendar-management-dialog.c',
  'gcal-calendar-management-page.c',
  'gcal-calendars-page.c',
  'gcal-edit-calendar-page.c',
  'gcal-file-chooser-button.c',
  'gcal-new-calendar-page.c',
)
