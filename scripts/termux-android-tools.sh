#!/data/data/com.termux/files/usr/bin/bash
# Discover Android tooling that actually executes on this Termux host.
fail_android_tool() {
  echo "Incompatible or missing Android tool: $1. In Termux install native packages (for example: pkg install zipalign apksigner aapt) or install matching Android build-tools; set ANDROID_HOME. Linux x86_64 SDK executables do not run on Android ARM64." >&2
  return 1
}
find_android_tool() {
  local name=$1 candidate
  if candidate=$(command -v "$name" 2>/dev/null) && [[ -x "$candidate" ]]; then
    if android_tool_works "$name" "$candidate"; then
      printf '%s\n' "$candidate"; return 0
    fi
  fi
  local sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}
  candidate="$sdk/build-tools/35.0.0/$name"
  if [[ -x "$candidate" ]] && android_tool_works "$name" "$candidate"; then
    printf '%s\n' "$candidate"; return 0
  fi
  fail_android_tool "$name"
}

# zipalign's help option prints usage but exits 2 in Termux. Exercise its
# alignment/check operations on a throwaway archive instead of relying on
# undocumented help/version exit codes.
android_tool_works() {
  local name=$1 candidate=$2 probe_dir
  case "$name" in
    zipalign)
      command -v zip >/dev/null 2>&1 || return 1
      probe_dir=$(mktemp -d) || return 1
      if (cd "$probe_dir" && printf 'probe\n' > probe.txt && zip -q probe.zip probe.txt &&
          "$candidate" -f -p 4 probe.zip aligned.zip >/dev/null 2>&1 &&
          "$candidate" -c 4 aligned.zip >/dev/null 2>&1); then
        rm -rf "$probe_dir"
        return 0
      fi
      rm -rf "$probe_dir"
      return 1
      ;;
    *)
      "$candidate" --help >/dev/null 2>&1 || "$candidate" version >/dev/null 2>&1 ||
        "$candidate" 2>&1 | head -n 2 | grep -qi "$name"
      ;;
  esac
}
