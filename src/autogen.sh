#!/bin/bash
rm -rf auto4mte.cache
AUTOGEN_TARGET=${AUTOGEN_TARGET-configure:config.h.in}
set -e
case :$AUTOGEN_TARGET: in
*:configure:*) autoconf; touch configure ;;
esac
case :$AUTOGEN_TARGET: in
*:config.h.in:*) autoheader; touch config.h.in ;;
esac
