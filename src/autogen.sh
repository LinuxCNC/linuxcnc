#!/bin/bash
rm -rf auto4mte.cache
AUTOGEN_TARGET=${AUTOGEN_TARGET-configure:config.h.in}
case :$AUTOGEN_TARGET: in
*:configure:*) autoconf ;;
esac
case :$AUTOGEN_TARGET: in
*:config.h.in:*) autoheader ;;
esac
