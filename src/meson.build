src_inc = include_directories('.')

enum_headers = files('gcal-utils.h')

libgcal_deps = [
  gsettings_desktop_schemas_dep,
  libedataserverui_dep,
  libedataserver_dep,
  libevolution_dep,
  libecal_dep,
  libsoup_dep,
  libical_dep,
  glib_dep,
  gtk_dep,
  gio_dep,
  goa_dep,
  m_dep,
]

sources = files(
  'views/gcal-month-view.c',
  'views/gcal-range-tree.c',
  'views/gcal-view.c',
  'views/gcal-week-grid.c',
  'views/gcal-week-header.c',
  'views/gcal-week-view.c',
  'views/gcal-year-view.c',
  'gcal-application.c',
  'gcal-clock.c',
  'gcal-date-chooser-day.c',
  'gcal-date-chooser.c',
  'gcal-date-selector.c',
  'gcal-edit-dialog.c',
  'gcal-event-widget.c',
  'gcal-event.c',
  'gcal-log.c',
  'gcal-manager.c',
  'gcal-multi-choice.c',
  'gcal-quick-add-popover.c',
  'gcal-recurrence.c',
  'gcal-search-view.c',
  'gcal-shell-search-provider.c',
  'gcal-source-dialog.c',
  'gcal-subscriber-view.c',
  'gcal-time-selector.c',
  'gcal-utils.c',
  'gcal-window.c',
)

gnome.mkenums(
  'org.gnome.calendar.enums.xml',
         sources: enum_headers,
        comments: '<!-- @comment@ -->',
           fhead: '<schemalist>',
           vhead: '  <@type@ id="org.gnome.calendar.@EnumName@">',
           vprod: '    <value nick="@valuenick@" value="@valuenum@"/>',
           vtail: '  </@type@>',
           ftail: '</schemalist>',
  install_header: true,
     install_dir: calendar_schemadir
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

incs = [
  top_inc,
  include_directories('views')
]

incs += contrib_incs


###################
# Private library #
###################

libgcal = shared_library(
               'gcal',
              sources: sources,
  include_directories: incs,
         dependencies: libgcal_deps
)

libgcal_dep = declare_dependency(
            link_with: libgcal,
  include_directories: src_inc
)


##################
# gnome-calendar #
##################

calendar_deps = libgcal_deps
calendar_deps += [ libgcal_dep ]

executable(
  meson.project_name(),
       files('main.c'),
   include_directories: incs,
          dependencies: calendar_deps,
               install: true,
           install_dir: calendar_bindir
)
