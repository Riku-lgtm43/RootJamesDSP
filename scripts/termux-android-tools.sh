#!/data/data/com.termux/files/usr/bin/bash
# Discover Android tooling that actually executes on this Termux host.
fail_android_tool() {
  echo "Incompatible or missing Android tool: $1. In Termux install native packages (for example: pkg install zipalign apksigner aapt) or install matching Android build-tools; set ANDROID_HOME. Linux x86_64 SDK executables do not run on Android ARM64." >&2
  return 1
}
find_android_tool() {
  local name=$1 candidate
  if candidate=$(command -v "$name" 2>/dev/null) && [[ -x "$candidate" ]]; then
    if "$candidate" --help >/dev/null 2>&1 || "$candidate" version >/dev/null 2>&1 || "$candidate" 2>&1 | head -n 2 | grep -qi "$name"; then
      printf '%s\n' "$candidate"; return 0
    fi
  fi
  local sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}
  candidate="$sdk/build-tools/35.0.0/$name"
  if [[ -x "$candidate" ]] && { "$candidate" --help >/dev/null 2>&1 || "$candidate" version >/dev/null 2>&1 || "$candidate" 2>&1 | head -n 2 | grep -qi "$name"; }; then
    printf '%s\n' "$candidate"; return 0
  fi
  fail_android_tool "$name"
}
