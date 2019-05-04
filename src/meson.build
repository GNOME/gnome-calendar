src_inc = include_directories('.')

gcal_deps = [
  gsettings_desktop_schemas_dep,
  libedataserverui_dep,
  libedataserver_dep,
  libevolution_dep,
  libecal_dep,
  libdazzle_dep,
  libsoup_dep,
  libical_dep,
  glib_dep,
  gtk_dep,
  gio_dep,
  goa_dep,
  gweather_dep,
  geoclue_dep,
  geocode_dep,
  m_dep,
]

sources = files(
  'core/gcal-calendar.c',
  'core/gcal-clock.c',
  'core/gcal-event.c',
  'core/gcal-manager.c',
  'core/gcal-recurrence.c',
  'core/gcal-timer.c',
  'gui/gcal-calendar-popover.c',
  'gui/gcal-date-chooser.c',
  'gui/gcal-date-chooser-day.c',
  'gui/gcal-date-selector.c',
  'gui/gcal-edit-dialog.c',
  'gui/gcal-event-widget.c',
  'gui/gcal-multi-choice.c',
  'gui/gcal-quick-add-popover.c',
  'gui/gcal-source-dialog.c',
  'gui/gcal-time-selector.c',
  'gui/gcal-window.c',
  'search/gcal-search-button.c',
  'search/gcal-search-engine.c',
  'search/gcal-search-hit.c',
  'search/gcal-search-hit-event.c',
  'search/gcal-search-model.c',
  'utils/gcal-date-time-utils.c',
  'utils/gcal-thread-utils.c',
  'utils/gcal-utils.c',
  'views/gcal-month-cell.c',
  'views/gcal-month-popover.c',
  'views/gcal-month-view.c',
  'views/gcal-range-tree.c',
  'views/gcal-view.c',
  'views/gcal-week-grid.c',
  'views/gcal-week-header.c',
  'views/gcal-week-view.c',
  'views/gcal-year-view.c',
  'weather/gcal-weather-info.c',
  'weather/gcal-weather-service.c',
  'weather/gcal-weather-settings.c',
  'gcal-application.c',
  'gcal-context.c',
  'gcal-log.c',
  'gcal-night-light-monitor.c',
  'gcal-shell-search-provider.c',
  'gcal-time-zone-monitor.c',
)

enum_types = 'gcal-enum-types'

sources += gnome.mkenums(
  enum_types,
     sources: enum_headers,
  c_template: enum_types + '.c.template',
  h_template: enum_types + '.h.template'
)

sources += gnome.compile_resources(
  'gcal-resources',
         gresource,
        source_dir: data_dir,
            c_name: 'calendar',
      dependencies: resource_data,
            export: true
)

sources += gnome.gdbus_codegen(
  'gcal-shell-search-provider-generated',
  shell_search_provider_dbus_interfaces,
  interface_prefix: 'org.gnome.',
         namespace: 'Gcal'
)

debug_conf = configuration_data()
debug_conf.set('BUGREPORT_URL', package_bugreport)
debug_conf.set10('ENABLE_TRACING', enable_tracing)


#########
# Debug #
#########

debug = 'gcal-debug.h'

sources += configure_file(
          input: debug + '.in',
         output: debug,
  configuration: debug_conf
)


#######################
# Include directories #
#######################

calendar_incs = [
  top_inc,
  include_directories('core'),
  include_directories('gui'),
  include_directories('search'),
  include_directories('utils'),
  include_directories('views'),
  include_directories('weather'),
]

calendar_incs += contrib_incs


###################
# Private library #
###################

libgcal = shared_library(
               'gcal-private',
              sources: sources,
  include_directories: calendar_incs,
         dependencies: gcal_deps,
              install: false,
)

libgcal_dep = declare_dependency(
            link_with: libgcal,
         dependencies: gcal_deps,
  include_directories: calendar_incs,
)


##################
# gnome-calendar #
##################

executable(
  meson.project_name(),
       sources + files('main.c'),
   include_directories: calendar_incs,
          dependencies: gcal_deps,
               install: true,
           install_dir: calendar_bindir
)
