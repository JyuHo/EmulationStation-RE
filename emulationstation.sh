#!/bin/sh
# Runs EmulationStation again when it asks for a restart (Quit menu > Restart).
# Reboot and power off are done by EmulationStation itself.

esdir="$(dirname "$0")"
rundir="${XDG_RUNTIME_DIR:-/tmp}"
case "$rundir" in /tmp) rundir="/tmp/emulationstation-$(id -u)" ;; *) rundir="$rundir/emulationstation" ;; esac

while true; do
    rm -f "$rundir/restart.please"
    "$esdir/emulationstation" "$@"
    ret=$?
    [ -f "$rundir/restart.please" ] || break
done
exit $ret
