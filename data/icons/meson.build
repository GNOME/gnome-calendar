icon_sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
  '256x256',
  '512x512'
]

foreach icon_size: icon_sizes
  install_data(
    '_'.join(['hicolor', 'apps', icon_size, 'org.gnome.Calendar.png']),
    install_dir: join_paths(calendar_datadir, 'icons', 'hicolor', icon_size, 'apps')
  )
endforeach

install_data(
  '_'.join(['hicolor', 'apps', 'symbolic', 'org.gnome.Calendar-symbolic.svg']),
  install_dir: join_paths(calendar_datadir, 'icons', 'hicolor', 'symbolic', 'apps')
)
