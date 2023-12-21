appdata_conf = configuration_data()
appdata_conf.set('appid', application_id)
appdata = i18n.merge_file(
  input: configure_file(
    input: files('org.gnome.Calendar.appdata.xml.in.in'),
    output: 'org.gnome.Calendar.appdata.xml.in',
    configuration: appdata_conf
  ),
  output: '@0@.appdata.xml'.format(application_id),
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
