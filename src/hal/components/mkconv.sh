#!/bin/sh

if [ $# -lt 2 ]; then
	echo "Too few arguments to $(basename "$0")." >&2
	echo "Usage: $(basename "$0") <from_type> <to_type>." >&2
	exit 1
fi

# Convert the hal type into the underlying type
utype() {
	case "$1" in
	"bool")	echo "rtapi_bool" ;;
	"sint")	echo "rtapi_sint" ;;
	"uint")	echo "rtapi_uint" ;;
	"real") echo "rtapi_real" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}

# Return the maximum value supported by a type
maxval() {
	case "$1" in
	"bool")	echo "1" ;;
	"sint")	echo "RTAPI_SINT_MAX" ;;
	"uint")	echo "RTAPI_UINT_MAX" ;;
	"real") echo "Never_Used" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}

# Return the minimum value supported by a type
minval() {
	case "$1" in
	"bool")	echo "0" ;;
	"sint")	echo "RTAPI_SINT_MIN" ;;
	"uint")	echo "0" ;;
	"real") echo "Never_Used" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}

# New HAL types
_newtype() {
	case "$1" in
	"bool")	echo "bool" ;;
	"sint")	echo "sint" ;;
	"uint")	echo "uint" ;;
	"real") echo "real" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}
#
# Conversions
# xxx = unsupported conversion
# o   = no bounds checks or clamp needed
#  +  = max side bound needed
#   - = min side bound needed
# (table: vertical=from ($1); horizontal=to ($2))
#      | real | uint | sint | bool
# -----+------+------+------+------
# real | xxx  |  +-  |  +-  | xxx
# uint | o    | xxx  |  +   |  +
# sint | o    |   -  | xxx  |  +-
# bool | o    | o    | o    | xxx
#
# Boolean implementation of the above table:
#      | real | uint | sint | bool
# -----+------+------+------+------
# real | o+-  |  +-  |  +-  | x+-
# uint | o    | x+x  |  +   |  +
# sint | o    |   -  | xx-  |  +-
# bool | o    | o    | o    | o+x
#

# Enable (val > MAX) test
V=1
test	"$1" = 'real' -o \
	"$2" = 'bool' -o \
	\( "$1" = 'uint' -a "$2" != 'real' \) && V=0
MAXEN="s,@MAXEN@,$V,g"

# Enable (val < MIN) test
V=1
test	"$1" = 'real' -o \
	\( "$1" = 'sint' -a "$2" != 'real' \) && V=0
MINEN="s,@MINEN@,$V,g"

# Disable clamp code
if test	"$2" = 'real' -o \
        "$1" = 'bool'; then
    CC="s,@CC@,//,g"
else
    CC="s,@CC@,,g"
fi

XIN="s,@XIN@,${1},g"
XOUT="s,@XOUT@,${2},g"
IN="s,@IN@,$(_newtype "$1"),g"
OUT="s,@OUT@,$(_newtype "$2"),g"
MIN="s,@MIN@,$(minval "$2"),g"
MAX="s,@MAX@,$(maxval "$2"),g"
TYPI="s,@TYPI@,$(utype "$1"),g"
TYPO="s,@TYPO@,$(utype "$2"),g"

exec sed -e "$IN; $OUT; $CC; $MIN; $MAX; $TYPI; $TYPO; $MINEN; $MAXEN; $XIN; $XOUT;"
