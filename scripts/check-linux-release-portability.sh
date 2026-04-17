#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  check-linux-release-portability.sh <binary> [--max-glibc <version>] [--allow <soname> ...]

Checks a Linux ELF release binary for two things:
  1. maximum referenced GLIBC symbol version
  2. unexpected direct runtime dependencies (NEEDED entries)

Always-allowed core ELF deps:
  - libc.so.6
  - libm.so.6
  - ld-linux-x86-64.so.2
EOF
}

version_gt() {
    local a="$1"
    local b="$2"
    [[ "$(printf '%s\n%s\n' "$a" "$b" | sort -V | tail -n1)" == "$a" && "$a" != "$b" ]]
}

if [[ $# -lt 1 ]]; then
    usage >&2
    exit 2
fi

BINARY=""
MAX_GLIBC=""
declare -a ALLOW_LIBS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --max-glibc)
            MAX_GLIBC="$2"
            shift 2
            ;;
        --allow)
            ALLOW_LIBS+=("$2")
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            if [[ -n "$BINARY" ]]; then
                echo "Only one binary path is supported" >&2
                usage >&2
                exit 2
            fi
            BINARY="$1"
            shift
            ;;
    esac
done

if [[ -z "$BINARY" ]]; then
    echo "Missing binary path" >&2
    usage >&2
    exit 2
fi

if [[ ! -f "$BINARY" ]]; then
    echo "Binary not found: $BINARY" >&2
    exit 2
fi

if ! file "$BINARY"; then
    echo "Failed to inspect ELF header for $BINARY" >&2
    exit 1
fi

echo "=== Direct runtime dependencies (NEEDED) ==="
mapfile -t NEEDED_LIBS < <(objdump -p "$BINARY" | awk '/NEEDED/ {print $2}')
printf '  %s\n' "${NEEDED_LIBS[@]}"

echo "=== Referenced GLIBC versions ==="
mapfile -t GLIBC_VERSIONS < <(strings -a "$BINARY" | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/^GLIBC_//' | sort -Vu)
if [[ ${#GLIBC_VERSIONS[@]} -eq 0 ]]; then
    echo "No GLIBC symbol versions found" >&2
    exit 1
fi
printf '  GLIBC_%s\n' "${GLIBC_VERSIONS[@]}"
MAX_FOUND_GLIBC="${GLIBC_VERSIONS[-1]}"
echo "Max GLIBC symbol: GLIBC_${MAX_FOUND_GLIBC}"

if [[ -n "$MAX_GLIBC" ]] && version_gt "$MAX_FOUND_GLIBC" "$MAX_GLIBC"; then
    echo "ERROR: $BINARY requires GLIBC_${MAX_FOUND_GLIBC}, which exceeds the allowed baseline GLIBC_${MAX_GLIBC}" >&2
    exit 1
fi

declare -a ALWAYS_ALLOWED=("libc.so.6" "libm.so.6" "ld-linux-x86-64.so.2")
declare -a UNEXPECTED=()

is_allowed() {
    local lib="$1"
    local allowed
    for allowed in "${ALWAYS_ALLOWED[@]}" "${ALLOW_LIBS[@]}"; do
        if [[ "$lib" == "$allowed" ]]; then
            return 0
        fi
    done
    return 1
}

for lib in "${NEEDED_LIBS[@]}"; do
    if ! is_allowed "$lib"; then
        UNEXPECTED+=("$lib")
    fi
done

if [[ ${#UNEXPECTED[@]} -gt 0 ]]; then
    echo "ERROR: unexpected direct runtime dependencies:" >&2
    printf '  %s\n' "${UNEXPECTED[@]}" >&2
    exit 1
fi

echo "Portability check passed"
