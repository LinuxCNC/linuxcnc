#!/bin/sh

set -eu #Needed so CI fails when anything is wrong
set -x

debian/configure

echo Changelog diff
diff debian/changelog-base debian/changelog
echo Version diff
diff VERSION_BASE VERSION

apt-get --yes build-dep --indep-only .
debuild -us -uc --build=source,all
