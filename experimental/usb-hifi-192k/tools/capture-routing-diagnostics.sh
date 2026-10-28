#!/data/data/com.termux/files/usr/bin/bash
# Read-only Android audio routing snapshot for JamesDSP USB HiFi investigations.
# Run while the player is actively playing the route/sample rate to be captured.

set -u

REPORT_DIR=${JAMESDSP_REPORT_DIR:-/sdcard/Download}
STAMP=$(date '+%Y%m%d_%H%M%S' 2>/dev/null || printf 'unknown-time')
REPORT="$REPORT_DIR/JamesDSP_USB_HiFi_Routing_$STAMP.txt"

if ! mkdir -p "$REPORT_DIR" 2>/dev/null || ! : > "$REPORT" 2>/dev/null; then
    printf 'Cannot write to %s. In Termux, run termux-setup-storage and allow access, then retry.\n' "$REPORT_DIR" >&2
    exit 2
fi

ROOT_UID=''
if command -v su >/dev/null 2>&1; then
    ROOT_UID=$(su -c 'id -u' 2>/dev/null || true)
fi
ROOT_AVAILABLE=0
if [[ "$ROOT_UID" == '0' ]]; then
    ROOT_AVAILABLE=1
fi

write_section() {
    {
        printf '\n\n===== %s =====\n' "$1"
        printf '%s\n' "$2"
    } >> "$REPORT"
}

run_android_command() {
    local command_text=$1
    local output status

    if [[ "$ROOT_AVAILABLE" == '1' ]]; then
        output=$(su -c "$command_text" 2>&1)
        status=$?
        if [[ $status -eq 0 && -n "$output" ]]; then
            printf '%s\n' "$output"
            return 0
        fi
        printf '[root command exit=%s]\n%s\n' "$status" "$output"
    fi

    output=$(sh -c "$command_text" 2>&1)
    status=$?
    if [[ $status -eq 0 && -n "$output" ]]; then
        printf '%s\n' "$output"
    else
        printf '[unprivileged command exit=%s]\n%s\n' "$status" "$output"
    fi
    return 0
}

redact() {
    sed -E \
        -e 's/[[:xdigit:]]{2}(:[[:xdigit:]]{2}){5}/[REDACTED-MAC]/g' \
        -e 's/([Ss][Ee][Rr][Ii][Aa][Ll]([Nn]umber)?[=:][[:space:]]*)[^[:space:],;]+/\1[REDACTED]/g' \
        -e 's/([Tt]oken|[Pp]assword|[Aa]uthorization|[Aa]ccess[_ -]?[Kk]ey)[=:][[:space:]]*[^[:space:],;]+/\1=[REDACTED]/g'
}

capture_filtered() {
    local title=$1
    local command_text=$2
    local pattern=$3
    local line_limit=$4
    local raw filtered

    raw=$(run_android_command "$command_text")
    filtered=$(printf '%s\n' "$raw" | redact | grep -Ein -C 2 "$pattern" | head -n "$line_limit" || true)
    if [[ -z "$filtered" ]]; then
        filtered=$(printf '%s\n' "$raw" | redact | head -n 60)
        [[ -n "$filtered" ]] || filtered='[No output; command unavailable, denied, or returned an empty dump.]'
    fi
    write_section "$title" "$filtered"
}

capture_full_limited() {
    local title=$1
    local command_text=$2
    local line_limit=$3
    local raw

    raw=$(run_android_command "$command_text")
    write_section "$title" "$(printf '%s\n' "$raw" | redact | head -n "$line_limit")"
}

{
    printf 'JamesDSP USB HiFi routing diagnostic\n'
    printf 'Captured: %s\n' "$(date '+%Y-%m-%d %H:%M:%S %z' 2>/dev/null || printf 'unknown')"
    printf 'Android release: %s\n' "$(getprop ro.build.version.release 2>/dev/null || printf 'unavailable')"
    printf 'Android SDK: %s\n' "$(getprop ro.build.version.sdk 2>/dev/null || printf 'unavailable')"
    printf 'Build fingerprint: %s\n' "$(getprop ro.build.fingerprint 2>/dev/null || printf 'unavailable')"
    printf 'Model: %s\n' "$(getprop ro.product.model 2>/dev/null || printf 'unavailable')"
    printf 'Root command available: %s\n' "$([[ "$ROOT_AVAILABLE" == '1' ]] && printf yes || printf no)"
    printf 'Capture note: start playback before running; each run records one route state.\n'
    printf 'This script only reads system state and writes this report file.\n'
} >> "$REPORT"

ROUTING_PATTERN='hifi|USB|Effect|effect|MIXER|Mixer|DUPLICATING|session|Session|track|Track|192000|96000|48000|44100|AUDIO_OUTPUT_FLAG|flags|strategy|route|device|Device|client|Client|format|Format|profile|Profile|active'

capture_filtered 'Audio policy: active routes, profiles, effect registrations' \
    'dumpsys media.audio_policy' "$ROUTING_PATTERN" 1600
capture_filtered 'AudioFlinger: playback threads, clients, formats, and effect chains' \
    'dumpsys media.audio_flinger' "$ROUTING_PATTERN" 2200
capture_filtered 'Audio service: active players and audio sessions' \
    'dumpsys audio' \
    'AudioPlaybackConfiguration|active player|PlayerProxy|session(Id| ID)|uid|pid|usage|content|sample.?rate|format|device|USB|hifi|192000|96000|48000|44100' 1400
capture_filtered 'Media sessions: active player identity/state' \
    'dumpsys media_session' \
    'active sessions|package|state=|PlaybackState|metadata|Apple|Music|playing' 500
capture_filtered 'USB audio status' \
    'dumpsys usb' \
    'USB|audio|device|interface|connected|CX31993|JCALLY' 500

capture_filtered 'Recent audio routing/effect logs (last 6000 log lines)' \
    'logcat -d -v threadtime -t 6000' \
    'AudioFlinger|AudioPolicyManager|AudioPolicyService|EffectModule|AudioEffect|effect.*(create|attach|move|config|compat|fail)|JamesDSP|JDSP|Axion[Ff]x|audio_hw|hifi_playback|USB-Audio' 1000

capture_full_limited 'ALSA USB stream descriptors (/proc/asound/card*/stream*)' \
    'for f in /proc/asound/card*/stream*; do if [ -r "$f" ]; then echo "--- $f ---"; cat "$f"; else echo "[unreadable or absent: $f]"; fi; done' 500

capture_full_limited 'Installed audio effect configuration candidates (read-only)' \
    'for f in /vendor/etc/audio_effects.xml /odm/etc/audio_effects.xml /system/vendor/etc/audio_effects.xml /data/adb/modules/jamesdsp_orbit_stage29/system/vendor/etc/audio_effects.xml /data/adb/ksu/modules/jamesdsp_orbit_stage29/system/vendor/etc/audio_effects.xml; do if [ -r "$f" ]; then echo "--- $f ---"; cat "$f"; fi; done; echo "--- module directories ---"; for d in /data/adb/modules/jamesdsp_orbit_stage29 /data/adb/ksu/modules/jamesdsp_orbit_stage29; do if [ -d "$d" ]; then ls -ld "$d"; fi; done' 700

printf '\nReport written: %s\n' "$REPORT"
