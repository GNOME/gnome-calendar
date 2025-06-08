blueprints = custom_target(
  'blueprints',
  input: files(
    'help-overlay.blp',
  ),
  output: '.',
  command: [
    find_program('blueprint-compiler'),
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)

built_sources += gnome.compile_resources(
  'gtk-resources',
  'gtk.gresource.xml',
  c_name: 'gtk',
  dependencies: blueprints,
)
