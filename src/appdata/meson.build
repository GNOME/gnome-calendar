appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)
appdata = i18n.merge_file(
  input: configure_file(
    input: files('org.gnome.Calendar.metainfo.xml.in.in'),
    output: 'org.gnome.Calendar.metainfo.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.metainfo.xml'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: calendar_datadir / 'metainfo'
)

# Validate Appdata
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'Validate appstream file',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appdata.full_path()],
    depends: appdata,
  )
endif

## GResource
gresource = configure_file(
  input: 'appdata.gresource.xml.in',
  output: 'appdata.gresource.xml',
  configuration: appdata_conf,
)

built_sources += gnome.compile_resources(
  'appdata-resources',
  gresource,
  c_name: 'appdata',
  dependencies: [gresource, appdata],
)
