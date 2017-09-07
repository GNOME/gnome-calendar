libevolution_files = files(
  'e-cal-data-model.c',
  'e-cal-data-model-subscriber.c'
)

libevolution_deps = [
  libecal_dep,
  libedataserver_dep,
]

libevolution = static_library(
      'evolution',
     dependencies: libevolution_deps,
          sources: libevolution_files,
              pic: true,
)

libevolution_dep = declare_dependency(
            link_with: libevolution,
  include_directories: include_directories('.'),
)
