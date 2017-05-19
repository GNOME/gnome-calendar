#!/usr/bin/env python3

import glob
import os
import re
import subprocess

install_prefix = os.environ['MESON_INSTALL_PREFIX']

name_pattern = re.compile('hicolor_apps_(?:\d+x\d+|symbolic)_(.*)')
search_pattern = '/**/hicolor_*'

icondir = os.path.join(install_prefix, 'share', 'icons', 'hicolor')
[os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))
 for file in glob.glob(icondir + search_pattern, recursive=True)]

schemadir = os.path.join(install_prefix, 'share', 'glib-2.0', 'schemas')

if not os.environ.get('DESTDIR'):
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

  print('Compiling gsettings schemas...')
  subprocess.call(['glib-compile-schemas', schemadir])
