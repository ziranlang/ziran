#!/bin/sh
set -eu
unused_ziran=${1:?pass the ziran command}
native_wx=${2:?pass the native W^X test}
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$native_wx"
