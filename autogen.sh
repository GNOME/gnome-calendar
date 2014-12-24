#!/bin/sh
# Run this to generate all the initial makefiles, etc.
set -e

autoreconf -i -f
intltoolize --force --copy --automake

if test -z "$NOCONFIGURE"; then
    run_configure=true
    for arg in $*; do
	case $arg in
            --no-configure)
		run_configure=false
		;;
            *)
		;;
	esac
    done
else
    run_configure=false
fi

if test $run_configure = true; then
    ./configure "$@"
fi
