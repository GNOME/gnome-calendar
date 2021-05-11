project(
  'gnome-calendar',
               'c',
           version: '40.1',
           license: 'GPL3+',
     meson_version: '>= 0.53.0'
)


###########
# Version #
###########

calendar_version = meson.project_version()
version_array = calendar_version.split('.')
calendar_major_version = version_array[0].to_int()
calendar_minor_version = version_array[1]


#################
# Default paths #
#################

calendar_prefix = get_option('prefix')
calendar_bindir = join_paths(calendar_prefix, get_option('bindir'))
calendar_localedir = join_paths(calendar_prefix, get_option('localedir'))
calendar_datadir = join_paths(calendar_prefix, get_option('datadir'))
calendar_pkgdatadir = join_paths(calendar_datadir, meson.project_name())
calendar_schemadir = join_paths(calendar_datadir, 'glib-2.0', 'schemas')


###########
# Options #
###########

if get_option('profile') == 'development'
  find_program('git')
  rev_txt = run_command('git','rev-parse','--short','HEAD').stdout().strip()
  profile = 'Devel'
  name_suffix = ' (Development)'
  rev = '-@0@'.format(rev_txt)
else
  profile = ''
  name_suffix = ''
  rev = ''
endif

if profile == ''
  application_id = 'org.gnome.Calendar'
else
  application_id = 'org.gnome.Calendar.@0@'.format(profile)
endif
calendar_buildtype = get_option('buildtype')
development_release = calendar_minor_version.startswith('alpha') or \
                      calendar_minor_version.startswith('beta') or \
                      calendar_minor_version.startswith('rc')
calendar_debug = development_release or calendar_buildtype.contains('debug')
enable_tracing = get_option('tracing')

cc = meson.get_compiler('c')

config_h = configuration_data()

package_bugreport = 'http://bugzilla.gnome.org/enter_bug.cgi?product=' + meson.project_name()

# package
set_defines = [
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_BUGREPORT', package_bugreport],
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), calendar_version)],
  ['PACKAGE_TARNAME', meson.project_name()],
  ['PACKAGE_URL', 'https://wiki.gnome.org/Apps/Calendar'],
  ['PACKAGE_VERSION', calendar_version + rev],
  ['VERSION', calendar_version + rev],
  ['GETTEXT_PACKAGE', meson.project_name()],
  ['PROFILE', profile],
  ['NAME_SUFFIX', name_suffix],
  ['APPLICATION_ID', application_id]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

assert(cc.has_function('strerror'), '"strerror" not found')

# options
config_h.set('ENABLE_TRACING', enable_tracing)
config_h.set('HANDY_USE_UNSTABLE_API', true)

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
nl_time_first_weekday_src = '''
  #include <langinfo.h>
  int main() {
    nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
  };
'''

config_h.set('HAVE__NL_TIME_FIRST_WEEKDAY', cc.compiles(nl_time_first_weekday_src),
         description: 'Define if _NL_TIME_FIRST_WEEKDAY is available')

# Not all systems support nl_langinfo (ALTMON_*)
altmon_src = '''
  /* _GNU_SOURCE is required by glibc 2.27.  */
  #define _GNU_SOURCE
  #include <langinfo.h>
  int main() {
    nl_langinfo (ALTMON_1);
    nl_langinfo (ALTMON_2);
    nl_langinfo (ALTMON_3);
    nl_langinfo (ALTMON_4);
    nl_langinfo (ALTMON_5);
    nl_langinfo (ALTMON_6);
    nl_langinfo (ALTMON_7);
    nl_langinfo (ALTMON_8);
    nl_langinfo (ALTMON_9);
    nl_langinfo (ALTMON_10);
    nl_langinfo (ALTMON_11);
    nl_langinfo (ALTMON_12);
  };
'''

config_h.set('HAVE_ALTMON', cc.compiles(altmon_src),
         description: 'Define if ALTMON_* constants are available')

# Compiler flags
common_flags = [
  '-DHAVE_CONFIG_H',
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(calendar_localedir),
  '-DPACKAGE_DATA_DIR="@0@"'.format(calendar_pkgdatadir),
  '-DUI_DATA_DIR="@0@"'.format(join_paths(calendar_datadir), 'style'),
  '-DEDS_DISABLE_DEPRECATED',
  '-DGOA_API_IS_SUBJECT_TO_CHANGE',
  '-DGWEATHER_I_KNOW_THIS_IS_UNSTABLE'
]

if calendar_debug
  common_flags += [
    '-DG_DISABLE_CAST_CHECKS'
  ]
elif calendar_buildtype == 'release'
  common_flags += [
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CHECKS',
    '-DG_DISABLE_CAST_CHECKS'
  ]
endif

add_project_arguments(common_flags, language: 'c')


################
# Dependencies #
################
libical_dep = dependency('libical', version: '>= 1.0')
config_h.set('HAVE_LIBICAL', libical_dep.found())

assert(cc.has_function('icaltime_days_in_year', dependencies: libical_dep),
       'Error: icaltime_days_in_year() not found in libical!. Upgrade your libical library.')

gsettings_desktop_schemas_dep = dependency('gsettings-desktop-schemas', version: '>= 3.21.2')
libedataserverui_dep = dependency('libedataserverui-1.2', version: '>= 3.17.1')
libedataserver_dep = dependency('libedataserver-1.2', version: '>= 3.17.1')
libecal_dep = dependency('libecal-2.0', version: '>= 3.33.2')
libsoup_dep = dependency('libsoup-2.4')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.33.1')
libhandy_dep = dependency('libhandy-1', version: '>= 1.0.0')
glib_dep = dependency('glib-2.0', version: '>= 2.67.5')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.20')
gio_dep = dependency('gio-2.0', version: '>= 2.58.0')
goa_dep = dependency('goa-1.0', version: '>= 3.2.0')
gweather_dep = dependency('gweather-3.0', version: '>= 3.27.2')
geoclue_dep = dependency('libgeoclue-2.0', version: '>=2.4')
geocode_dep = dependency('geocode-glib-1.0', version: '>=3.23')
m_dep = cc.find_library('m')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

top_inc = include_directories('.')

data_dir = join_paths(meson.source_root(), 'data')
po_dir = join_paths(meson.source_root(), 'po')
src_dir = join_paths(meson.source_root(), 'src')


###########
# Subdirs #
###########

subdir('build-aux')
subdir('data')
subdir('src')
subdir('po')
subdir('tests')

configure_file(
         output: 'config.h',
  configuration: config_h
)

summary({
  'Source': meson.source_root(),
  'Prefix': calendar_prefix,
})
summary({
  'Debug': calendar_debug,
  'Tracing': enable_tracing,
}, section: 'Development')
