test_deps = gcal_deps + [ libgcal_dep ]

###########
# Manager #
###########

test_env = [
         'G_TEST_SRCDIR=' + meson.current_source_dir(),
       'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  'GSETTINGS_SCHEMA_DIR=' + join_paths(meson.build_root(), 'data'),
     'GSETTINGS_BACKEND=memory',
         'MALLOC_CHECK_=2'
]

test_cflags = '-DTEST_DATA_DIR="@0@"'.format(join_paths(meson.source_root(), 'data'))

test_unit = 'test-manager'

test_manager = executable(
     test_unit,
  [test_unit + '.c', compiled_schemas],
        c_args: test_cflags,
  dependencies: test_deps
)

test(test_unit, test_manager, env: test_env)
