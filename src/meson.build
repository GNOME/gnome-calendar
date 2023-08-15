src_inc = include_directories('.')

gcal_deps = [
  libadwaita_dep,
  gsettings_desktop_schemas_dep,
  libedataserverui_dep,
  libedataserver_dep,
  libecal_dep,
  libsoup_dep,
  libical_dep,
  glib_dep,
  gtk_dep,
  gio_dep,
  gweather_dep,
  geoclue_dep,
  m_dep,
]

enum_types = 'gcal-enum-types'

built_sources = gnome.mkenums(
  enum_types,
     sources: enum_headers,
  c_template: enum_types + '.c.template',
  h_template: enum_types + '.h.template'
)

built_sources += gnome.gdbus_codegen(
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

built_sources += configure_file(
          input: debug + '.in',
         output: debug,
  configuration: debug_conf
)


#######################
# Include directories #
#######################

calendar_incs = [
  top_inc,
  include_directories('.'),
]

###################
# Private library #
###################

sources = files()
subdir('core')
subdir('gui')
subdir('search')
subdir('theme')
subdir('utils')
subdir('weather')

libgcal_generated_sources_dep = declare_dependency(
  sources: built_sources,
)

gcal_deps += libgcal_generated_sources_dep

libgcal = static_library(
               'gcal-private',
              sources: sources,
  include_directories: calendar_incs,
         dependencies: gcal_deps,
              install: false,
)

libgcal_dep = declare_dependency(
            link_with: libgcal,
              sources: built_sources,
         dependencies: gcal_deps,
  include_directories: calendar_incs,
)


##################
# gnome-calendar #
##################

executable(
  meson.project_name(),
       files('main.c'),
          dependencies: libgcal_dep,
               install: true,
           install_dir: calendar_bindir,
)
