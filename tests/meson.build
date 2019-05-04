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

#########
# Event #
#########

test_event = executable(
     'test-event',
    ['test-event.c', compiled_schemas],
        c_args: test_cflags,
  dependencies: libgcal_test_dep,
)

test('test-event', test_event, env: test_env)


###########
# Manager #
###########

test_manager = executable(
     'test-manager',
    ['test-manager.c', compiled_schemas],
        c_args: test_cflags,
  dependencies: libgcal_test_dep,
)

test('test-manager', test_manager, env: test_env)
