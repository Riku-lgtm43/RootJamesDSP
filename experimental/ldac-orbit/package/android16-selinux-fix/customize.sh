#!/system/bin/sh
# Sourced by KernelSU after extraction. No direct system partition writes.
[ "$ARCH" = arm64 ] || abort '! This build supports arm64 only.'
[ -d "$MODPATH/system/vendor/lib64/soundfx" ] || abort '! Missing vendor soundfx directory.'
[ -f "$MODPATH/system/vendor/etc/audio_effects.xml" ] || abort '! Missing vendor audio effects XML.'
if [ -d /data/adb/modules/jamesdsp_orbit_stage23 ] && [ ! -f /data/adb/modules/jamesdsp_orbit_stage23/disable ]; then
  abort '! Disable JamesDSP Stage23 in KernelSU before installing Stage31.'
fi
ui_print '- Setting vendor_file labels on native libraries'
set_perm_recursive "$MODPATH/system/vendor" 0 0 0755 0644 u:object_r:vendor_file:s0
ui_print '- Setting vendor_configs_file labels on /vendor/etc configuration and HRTF data'
set_perm_recursive "$MODPATH/system/vendor/etc" 0 0 0755 0644 u:object_r:vendor_configs_file:s0
ui_print '- No SELinux policy changes, no live partition writes'
ui_print '- Reboot required: verify labels and effect registration after boot'
