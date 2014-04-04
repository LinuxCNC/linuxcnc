#!/bin/bash

thisfile=$(readlink -f "$0")
thisdir=$(dirname "$thisfile")
cd "$thisdir"

LAYOUT=${1:-""}

[ -x "$(which $COLORTERM)" ] && TERM=$COLORTERM
[ -z $TERM ] && TERM=xterm

XHC_HB04=$(which xhc-hb04)

if [ ! -z "$LAYOUT" ] ; then
   dashI="-I $LAYOUT"
fi

if [ ! -z "$debug" ] ; then
  echo "COLORTERM=$COLORTERM"
  echo "TERM=$TERM"
  echo "XHC_HB04=$XHC_HB04"
  echo "LAYOUT=$LAYOUT"
fi
$TERM -e "$XHC_HB04 -x $dashI"

