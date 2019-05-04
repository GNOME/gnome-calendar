test_deps = gcal_deps + [ libgcal_dep ]


test_env = [
         'G_TEST_SRCDIR=' + meson.current_source_dir(),
       'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GSETTINGS_SCHEMA_DIR=' + join_paths(meson.build_root(), 'data'),
     'GSETTINGS_BACKEND=memory',
         'MALLOC_CHECK_=2'
]

test_cflags = '-DTEST_DATA_DIR="@0@"'.format(join_paths(meson.source_root(), 'data'))


#########
# Event #
#########

test_event = executable(
     'test-event',
    ['test-event.c', compiled_schemas],
               c_args: test_cflags,
         dependencies: test_deps,
  include_directories: calendar_incs,
)

test('test-event', test_event, env: test_env)


###########
# Manager #
###########

test_manager = executable(
     'test-manager',
    ['test-manager.c', compiled_schemas],
               c_args: test_cflags,
         dependencies: test_deps,
  include_directories: calendar_incs,
)

test('test-manager', test_manager, env: test_env)
