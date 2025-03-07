#!/bin/bash

if ! command -v cppcheck > /dev/null; then
  echo "E: Please install the program 'cppcheck' prior to executing this script."
  exit 1
fi

nproc=2
if command -v nproc > /dev/null; then
  nproc=$(nproc)
fi

# See if cppcheck accepts --check-level
EXHAUSTIVE=$(cppcheck --check-level=exhaustive --version > /dev/null 2>&1 && echo "--check-level=exhaustive")

CPPCHKOPT=( -j "$nproc" --force "$EXHAUSTIVE" --inline-suppr )
CPPCHKOPT+=( "--enable=warning,performance,portability" )
CPPCHKOPT+=( "-I$(realpath "$(dirname "$0")/../include")" )

if [ -n "$CPPCHECK_OPTS" ]; then
    read -r -a OPTS <<< "$CPPCHECK_OPTS"
    CPPCHKOPT+=( "${OPTS[@]}" )
fi

# Even cppcheck 2.3 (debian 11) supports c++17 (undocumented)
CCSTD=( --std=c11 --language=c )
CXSTD=( --std=c++17 --language=c++ )

CPPCHKCC=( "${CPPCHKOPT[@]}" "${CCSTD[@]}" )
CPPCHKCX=( "${CPPCHKOPT[@]}" "${CXSTD[@]}" )

# Do this from the source directory
cd "$(dirname "$0")/../src" || { echo "Could not change directory to '$(dirname "$0")/../src'"; exit 1; }

# Only process individual files if passed on the command line.
if [ $# -gt 0 ]; then
    retval=0
    for f in "$@"; do
        if [ -r "$f" ]; then
            case "$f" in
            *.hh|*.cc)
                cppcheck -I"$(dirname "$f")" "${CPPCHKCX[@]}" "$f" || retval=1
                ;;
            *.h|*.c)
                cppcheck -I"$(dirname "$f")" "${CPPCHKCC[@]}" "$f" || retval=1
                ;;
            esac
        else
            echo "Cannot read file '$f'"
            retval=1
        fi
    done
    exit $retval
fi

docheck() {
    local rv
    rv=0
    mapfile -t files < <(find "$1" -maxdepth 1 -name "*.c")
    if [ "${#files[@]}" -gt 0 ]; then
        cppcheck -I"$1" "${CPPCHKCC[@]}" "${files[@]}" || rv=1
    fi
    mapfile -t files < <(find "$1" -maxdepth 1 -name "*.cc")
    if [ "${#files[@]}" -gt 0 ]; then
        cppcheck -I"$1" "${CPPCHKCX[@]}" "${files[@]}" || rv=1
    fi
    return $rv
}

# Collected returns; will be zero if all succeed
result=0

# *** HAL files ***
echo "I (1/4): checking HAL folders with both C and C++ code"
while IFS= read -r -d '' d
do
    # Don't care about examples
    case "$d" in hal/user_comps/mb2hal/examples*) continue;; esac

    echo "I (1/4): checking $d"
    docheck "$d" || result=1
done < <(find hal/ -type d -not -name "*__pycache__" -print0)

# *** EMC files ***
echo "I (2/4): checking EMC folders with both C and C++ code"
while IFS= read -r -d '' d
do
    # Will give Tcl problems
    case "$d" in emc/usr_intf/axis/extensions*) continue;; esac

    echo "I (2/4): checking $d"
    docheck "$d" || result=1
done < <(find emc/ -type d -not -name "*__pycache__" -print0)

# *** NML files ***
echo "I (3/4): checking LIBNML folders with both C and C++ code"
while IFS= read -r -d '' d
do
    echo "I (3/4): checking $d"
    docheck "$d" || result=1
done < <(find libnml/ -type d -not -name "*__pycache__" -print0)

# *** RTAPI files ***
echo "I (4/4): checking RTAPI folders with both C and C++ code"
while IFS= read -r -d '' d
do
    echo "I (4/4): checking $d"
    docheck "$d" || result=1
done < <(find rtapi/ -type d -not -name "*__pycache__" -print0)

exit $result
