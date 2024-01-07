#!/bin/bash
thisdir=$(dirname $0)
cd $thisdir

echo "================================================="
echo "Changelog:"
head -10 changelog
echo "..."
echo "================================================="
echo
echo "Is changelog up-to-date? (y|n)"
read ans
case $ans in
  y*|Y*) echo Building deb;;
  *)     echo bye; echo; exit 1;;
esac
# make the debs:
# -us Do not sign the source package
# -uc Do not sign the .changes file
(cd ..; debuild  -us -uc -b)
status=$?
echo
case $status in
  0) echo; echo $(head -1 changelog);
     echo "deb made: $(cd ../..; pwd; ls -l nativecam*.deb)"
     ;;
  2) echo "failed to build deb package";;
  *) echo "unexpected status $status";;
esac
