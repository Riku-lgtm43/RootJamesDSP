#!/data/data/com.termux/files/usr/bin/bash
# Print installed, absent, or unknown for a package name. An empty successful
# registry result confirms absence; command failures never imply absence.
termux_package_status() {
  local package=$1 output tool
  local -a attempts=()

  if tool=$(command -v dumpsys 2>/dev/null); then
    attempts+=("$tool")
  fi
  [[ -x /system/bin/dumpsys ]] && attempts+=(/system/bin/dumpsys)

  for tool in "${attempts[@]}"; do
    if output=$("$tool" package 2>&1); then
      if printf '%s\n' "$output" | grep -Fq "Package [$package]"; then
        printf 'installed\n'
      else
        printf 'absent\n'
      fi
      return 0
    fi
  done

  if command -v su >/dev/null 2>&1; then
    if output=$(su -c '/system/bin/dumpsys package' 2>&1); then
      if printf '%s\n' "$output" | grep -Fq "Package [$package]"; then
        printf 'installed\n'
      else
        printf 'absent\n'
      fi
      return 0
    fi
    if output=$(su -c 'dumpsys package' 2>&1); then
      if printf '%s\n' "$output" | grep -Fq "Package [$package]"; then
        printf 'installed\n'
      else
        printf 'absent\n'
      fi
      return 0
    fi
  fi

  printf 'unknown\n'
}
