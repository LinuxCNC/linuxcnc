#!/bin/bash
# Shared "skip" predicate for ui-smoke tests.
# runtests semantics: a `skip` script that returns non-zero causes the
# test to be skipped. Per-test skip files invoke this.
#
# We only skip on xvfb-run absence (rare local dev env). Python /
# typelib deps are declared in debian/control under !nocheck so CI
# always has them; missing deps should fail the test loudly rather
# than silently skip (BsAtHome / hdiethelm review, PR #3999).
set -u

if ! command -v xvfb-run >/dev/null 2>&1; then
    echo "skip: xvfb-run not installed" >&2
    exit 1
fi

exit 0
