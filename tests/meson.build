test_env = [
         'G_TEST_SRCDIR=' + meson.current_source_dir(),
       'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GSETTINGS_SCHEMA_DIR=' + join_paths(meson.build_root(), 'data'),
     'GSETTINGS_BACKEND=memory',
         'MALLOC_CHECK_=2'
]

test_cflags = '-DTEST_DATA_DIR="@0@"'.format(join_paths(meson.source_root(), 'data'))

################
# libgcal-test #
################

sources = files(
  'gcal-simple-server.c',
  'gcal-stub-calendar.c',
)

libgcal_test = shared_library(
               'gcal-test-private',
              sources: sources,
  include_directories: calendar_incs,
         dependencies: libgcal_dep,
              install: false,
)

libgcal_test_dep = declare_dependency(
            link_with: libgcal_test,
         dependencies: libgcal_dep,
  include_directories: calendar_incs,
)

tests = [
  'server',
  'event',
  'manager',
]

foreach test : tests
  test_name = 'test-@0@'.format(test)
  test_source = ['@0@.c'.format(test_name)]

  test_executable = executable(
       test_name,
      [test_source, compiled_schemas],
          c_args: test_cflags,
    dependencies: libgcal_test_dep,
  )

  test(test, test_executable, env: test_env)
endforeach
