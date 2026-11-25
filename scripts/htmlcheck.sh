#!/bin/bash

usage () {
    P=${0##*/}
    cat <<EOF
$P: Run html checks

Usage:
    $P [-v] [-e] [file ...]
    Run html checks

    -w: Warn only
        Returns zero on check error but shows warning and
        generates CI annotation
    -v: Verbose
    -e: Check external links
EOF
}

WARN=0
VERBOSE=0
EXTERNAL=0
while getopts wveh opt; do
    case "$opt" in
    w) WARN=1 ;;
    v) VERBOSE=1 ;;
    e) EXTERNAL=1 ;;
    h|?) usage; exit 0 ;;
    *) usage; exit 1 ;;
    esac
done
shift $((OPTIND-1))

if ! command -v checklink > /dev/null; then
    echo "ERROR: checklink not found in PATH, install w3c-linkchecker for HTML link validation" 1>&2
    echo "Can be downloaded from https://github.com/w3c/link-checker if there is no package for your distribution." 1>&2
    echo "link-checker/bin/checklink is a perl script, nothing else is needed from the above repo." 1>&2
    exit 1
fi

CHKOPT=( --follow-file-links )
if [ "$VERBOSE" -eq 0 ]; then
    CHKOPT+=( --quiet )
fi
if [ "$EXTERNAL" -eq 0 ]; then
    CHKOPT+=( --exclude "(http|https|irc)://" )
fi

#Note: grep is used to filer out an error message due to a bug in checklink in debian/ubuntu
#should be removed as soon as this package is fixed (upstream is already fine)

warnval=0
retval=0
if [ $# -gt 0 ]; then
    # Only process individual files if passed on the command line.
    for f in "$@"; do
        if [ -r "$f" ]; then
            checklink "${CHKOPT[@]}" "$f" 2>&1 | grep -vE 'Use of uninitialized value .* at .*checklink line [0-9]+'
            ret="${PIPESTATUS[0]}"
            if [ "$ret" -ne 0 ]; then
                echo "'$f': File check: Fail"
                if [ $WARN != 0 ]; then
                    warnval="$ret"
                else
                    retval="$ret"
                fi
            else
                echo "'$f': File check: OK"
            fi
        else
            echo "Cannot read file '$f'"
            retval=1
        fi
    done
else
    #Otherwhise, recursively check docs/build/html/index.html
    f="$(dirname "$0")/../docs/build/html/index.html"
    if [ -r "$f" ]; then
        CHKOPT+=( --recursive )
        checklink "${CHKOPT[@]}" "$f" 2>&1 | grep -vE 'Use of uninitialized value .* at .*checklink line [0-9]+'
        ret="${PIPESTATUS[0]}"
        if [ "$ret" -ne 0 ]; then
            echo "'$f': Recursive check: Fail"
            if [ $WARN != 0 ]; then
                warnval="$ret"
            else
                retval="$ret"
            fi
        else
            echo "'$f': Recursive check: OK"
        fi
    else
        echo "Cannot read file '$f', did you build the doc first?"
        retval=1
    fi
fi

#Generate a CI warning or error if running in CI
#See: https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-commands
if [ -n "${GITHUB_ACTIONS:-}" ]; then
    if [ "$retval" != 0 ]; then
        echo "::error title=HTML checks failed::checks failed"
    elif [ "$warnval" != 0 ]; then
        echo "::warning title=HTML checks failed::checks failed"
    fi
fi

exit "$retval"
