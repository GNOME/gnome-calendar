#!/usr/bin/env bash

if [ "$1" = "--quit" -o "$1" = "-q" ]; then
    /app/bin/gnome-calendar "$@"
    exit
fi

export GIO_USE_NETWORK_MONITOR=base
gsettings reset org.gnome.evolution-data-server network-monitor-gio-name
/app/libexec/evolution-source-registry &
sleep 1
/app/libexec/evolution-addressbook-factory -r &
/app/libexec/evolution-calendar-factory -r &
sleep 1
/app/bin/gnome-calendar "$@"
