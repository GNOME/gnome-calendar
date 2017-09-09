test_incs = [
  include_directories('.'),
  src_inc
]

tests_deps = gcal_deps + [ libgcal_dep ]

###########
# Manager #
###########

test_manager = executable(
     'test-manager',
     'test-manager.c',
         dependencies: tests_deps,
  include_directories: test_incs
)

test('test-manager', test_manager)
