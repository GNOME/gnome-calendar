appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)
appdata = i18n.merge_file(
  'appdata',
  input: configure_file(
    input: files('org.gnome.Calendar.appdata.xml.in.in'),
    output: 'org.gnome.Calendar.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(calendar_datadir, 'metainfo')
)
