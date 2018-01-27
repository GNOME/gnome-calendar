subdir('icons')
subdir('appdata')

# Desktop files
# Note: This name is registered in gcal-weather-service.c.
desktop = 'org.gnome.Calendar.desktop'

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(calendar_datadir, 'applications')
)

# GNOME Shell search provider
install_data(
  'org.gnome.Calendar.search-provider.ini',
  install_dir: join_paths(calendar_datadir, 'gnome-shell', 'search-providers')
)

# GSettings schema
install_data(
  'org.gnome.calendar.gschema.xml',
  install_dir: calendar_schemadir
)

# DBus service files
service_conf = configuration_data()
service_conf.set('bindir', calendar_bindir)

service = 'org.gnome.Calendar.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(calendar_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

shell_search_provider_dbus_interfaces = files('shell-search-provider-dbus-interfaces.xml')

# Resource data
gresource = files('calendar.gresource.xml')

resource_data = files(
  'theme/Adwaita.css',
  'ui/alarm-row.ui',
  'ui/calendar-row.ui',
  'ui/date-chooser.ui',
  'ui/date-selector.ui',
  'ui/edit-dialog.ui',
  'ui/help-overlay.ui',
  'ui/menus.ui',
  'ui/month-cell.ui',
  'ui/month-popover.ui',
  'ui/month-view.ui',
  'ui/multi-choice.ui',
  'ui/quick-add-popover.ui',
  'ui/search-view.ui',
  'ui/source-dialog.ui',
  'ui/time-selector.ui',
  'ui/weather-settings.ui',
  'ui/week-header.ui',
  'ui/week-view.ui',
  'ui/window.ui',
  'ui/year-view.ui'
)
