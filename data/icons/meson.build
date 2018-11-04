symbolic_icondir = join_paths('hicolor', 'symbolic', 'apps')

install_data(
  join_paths(symbolic_icondir, 'org.gnome.Calendar-symbolic.svg'),
  install_dir: join_paths(
    calendar_datadir,
    join_paths('icons', symbolic_icondir)
  ),
  rename: '@0@-symbolic.svg'.format(application_id)
)

icondir = join_paths('hicolor', 'scalable', 'apps')

install_data(
  join_paths(icondir, 'org.gnome.Calendar.svg'),
  install_dir: join_paths(
    calendar_datadir,
    join_paths('icons', icondir)
  ),
  rename: '@0@.svg'.format(application_id)
)
