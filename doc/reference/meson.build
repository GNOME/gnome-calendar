subdir('xml')

html_images = [
  'images/gcal-edit-dialog.png',
  'images/gcal-event-widget.png',
  'images/gcal-source-dialog.png',
  'images/gcal-window.png',
  'images/gcal-window_agendas.png'
]

version_conf = configuration_data()
version_conf.set('VERSION', calendar_version)

version = 'version.xml'

version_xml = configure_file(
          input: version + '.in',
         output: version,
  configuration: version_conf
)

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_doc_path = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_doc_path = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

doc_path = join_paths(calendar_datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
          main_sgml: meson.project_name() + '-docs.sgml',
            src_dir: src_inc,
       dependencies: libgcal_dep,
          scan_args: '--rebuild-types',
          mkdb_args: [ '--sgml-mode', '--output-format=xml' ],
  gobject_typesfile: meson.project_name() + '.types',
        html_assets: html_images,
            install: true,
        install_dir: doc_path
       fixxref_args: [
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'evolution-data-server')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'glib')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gobject')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gio')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gdk')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gtk'))
  ],
)
