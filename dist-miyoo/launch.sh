#!/bin/sh
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE" || exit 1
HOME="$HERE"
export HOME

# audio goes through SDL's OSS backend against the /dev/dsp shim provided
# by Allium's libpadsp.so preload
: "${SDL_AUDIODRIVER:=dsp}"
LD_LIBRARY_PATH="/lib:/config/lib:/customer/lib:/mnt/SDCARD/miyoo:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte:/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
if [ -z "${LD_PRELOAD:-}" ]; then
  for padsp in \
    /customer/lib/libpadsp.so \
    /mnt/SDCARD/.tmp_update/lib/libpadsp.so \
    /mnt/SDCARD/miyoo/lib/libpadsp.so \
    /lib/libpadsp.so
  do
    if [ -f "$padsp" ]; then
      LD_PRELOAD="$padsp"
      break
    fi
  done
fi
export LD_LIBRARY_PATH SDL_AUDIODRIVER LD_PRELOAD

if [ -x /mnt/SDCARD/.tmp_update/script/set_sound_level.sh ]; then
  /mnt/SDCARD/.tmp_update/script/set_sound_level.sh &
fi

ORIGINAL_CPU_GOVERNOR=""
if [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
  ORIGINAL_CPU_GOVERNOR=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)
  echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
fi

./metalmutant "$@"
status=$?

if [ -n "$ORIGINAL_CPU_GOVERNOR" ]; then
  echo "$ORIGINAL_CPU_GOVERNOR" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || true
fi
exit "$status"
