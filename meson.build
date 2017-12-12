project(
  'gnome-calendar',
               'c',
           version: '3.27.2',
           license: 'GPL3+',
   default_options: [ 'buildtype=debugoptimized', 'warning_level=1' ],
     meson_version: '>= 0.42.0'
)


###########
# Version #
###########

calendar_version = meson.project_version()
version_array = calendar_version.split('.')
calendar_major_version = version_array[0].to_int()
calendar_minor_version = version_array[1].to_int()
calendar_micro_version = version_array[2].to_int()


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

calendar_buildtype = get_option('buildtype')
calendar_debug = calendar_minor_version.is_odd() or buildtype.contains('debug')
enable_tracing = get_option('enable-tracing')
enable_gtk_doc = get_option('enable-gtk-doc')

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
  ['PACKAGE_VERSION', calendar_version],
  ['VERSION', calendar_version],
  ['GETTEXT_PACKAGE', meson.project_name()]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# headers
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_LANGINFO_H', 'langinfo.h'],
  ['HAVE_LOCALE_H', 'locale.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h']
]

foreach header: check_headers
  config_h.set(header[0], cc.has_header(header[1]))
endforeach

# functions
check_functions = [
  ['HAVE_DCGETTEXT', 'dcgettext'],
  ['HAVE_GETTEXT', 'gettext'],
  ['HAVE_FLOOR', 'floor'],
  ['HAVE_ICONV', 'iconv'],
  ['HAVE_MEMSET', 'memset'],
  ['HAVE_NL_LANGINFO', 'nl_langinfo'],
  ['HAVE_POW', 'pow'],
  ['HAVE_SETLOCALE', 'setlocale'],
  ['HAVE_STRSTR', 'strstr']
]

if host_machine.system().contains('darwin')
  check_functions += [
    ['HAVE_CFLOCALECOPYCURRENT', 'CFLocaleCopyCurrent'],
    ['HAVE_CFPREFERENCESCOPYAPPVALUE', 'CFPreferencesCopyAppValue']
  ]
endif

foreach func: check_functions
  config_h.set(func[0], cc.has_function(func[1]))
endforeach

assert(cc.has_function('strerror'), '"strerror" not found')

# options
config_h.set('ENABLE_TRACING', enable_tracing)

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
nl_time_first_weekday_src = '''
  #include <langinfo.h>
  int main() {
    nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
  };
'''

config_h.set('HAVE__NL_TIME_FIRST_WEEKDAY', cc.compiles(nl_time_first_weekday_src),
         description: 'Define if _NL_TIME_FIRST_WEEKDAY is available')

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

test_cflags = ['-Wno-sign-compare']

foreach cflag: test_cflags
  if cc.has_argument(cflag)
    common_flags += [cflag]
  endif
endforeach

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
libecal_dep = dependency('libecal-1.2', version: '>= 3.13.90')
libsoup_dep = dependency('libsoup-2.4')
libdazzle_dep = dependency('libdazzle-1.0', version: '>= 3.26.1')
glib_dep = dependency('glib-2.0', version: '>= 2.43.4')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22.20')
gio_dep = dependency('gio-2.0', version: '>= 2.43.4')
goa_dep = dependency('goa-1.0', version: '>= 3.2.0')
gweather_dep = dependency('gweather-3.0', version: '>= 3.27.2')
geoclue_dep = dependency('libgeoclue-2.0', version: '>=2.4')
geocode_dep = dependency('geocode-glib-1.0', version: '>=3.23')
m_dep = cc.find_library('m')

configure_file(
         output: 'config.h',
  configuration: config_h
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

top_inc = include_directories('.')

data_dir = join_paths(meson.source_root(), 'data')
po_dir = join_paths(meson.source_root(), 'po')


###########
# Subdirs #
###########

subdir('contrib')
subdir('data')
subdir('src')
subdir('po')
subdir('tests')

if enable_gtk_doc
  subdir('doc/reference')
endif

meson.add_install_script('meson_post_install.py')

output = '\n\n      GNOME Calendar ' + calendar_version + '\n'
output += '      =========================\n\n'
output += '          Source ..........................: ' + meson.source_root() + '\n'
output += '          Prefix ..........................: ' + calendar_prefix + '\n'
output += '          Compiler ........................: ' + cc.get_id() + '\n\n'
output += '      Development options\n'
output += '          Enable Debug: ...................: ' + calendar_debug.to_string() + '\n'
output += '          Enable Tracing: .................: ' + enable_tracing.to_string() + '\n'
output += '          Enable Documentation: ...........: ' + enable_gtk_doc.to_string() + '\n\n'
output += '    Now type "ninja -C ' + meson.build_root() + '" to build ' + meson.project_name() + '\n\n'
message(output)
