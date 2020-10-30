calendar_incs +=  include_directories('.')

built_sources += gnome.compile_resources(
  'weather-resources',
  'weather.gresource.xml',
  c_name: 'weather',
)

sources += files(
  'gcal-weather-info.c',
  'gcal-weather-service.c',
  'gcal-weather-settings.c',
)
