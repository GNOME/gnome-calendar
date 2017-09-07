test_incs = [
  include_directories('.'),
  src_inc
]

###########
# Manager #
###########

test_manager = executable(
     'test-manager',
     'test-manager.c',
         dependencies: calendar_deps,
  include_directories: test_incs
)

test('test-manager', test_manager)
