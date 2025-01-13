#!/bin/bash

if [ $# -lt 2 ]; then
	echo "Too few arguments to $(basename "$0")." >&2
	echo "Usage: $(basename "$0") <from_type> <to_type>." >&2
	exit 1
fi

# Convert the hal type into the underlying type
utype() {
	case "$1" in
	"bit")	echo "bool" ;;
	"s32")	echo "rtapi_s32" ;;
	"u32")	echo "rtapi_u32" ;;
	"s64")	echo "rtapi_s64" ;;
	"u64")	echo "rtapi_u64" ;;
	"float") echo "real_t" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}

# Return the maximum value supported by a type
maxval() {
	case "$1" in
	"bit")	echo "1" ;;
	"s32")	echo "INT32_MAX" ;;
	"u32")	echo "UINT32_MAX" ;;
	"s64")	echo "INT64_MAX" ;;
	"u64")	echo "UINT64_MAX" ;;
	"float") echo "Never_Used" ;;
	*)	echo "This_Will_Generate_An_Error" ;;
	esac
}

# Return the minimum value supported by a type
minval() {
	case "$1" in
	"bit")	echo "0" ;;
	"s32")	echo "INT32_MIN" ;;
	"u32")	echo "0" ;;
	"s64")	echo "INT64_MIN" ;;
	"u64")	echo "0" ;;
	"float") echo "Never_Used" ;;
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
#     | flt | u32 | s32 | u64 | s64 | bit
# ----+-----+-----+-----+-----+-----+-----
# flt | xxx |  +- |  +- |  +- |  +- | xxx
# u32 | o   | xxx |  +  | o   | o   |  +
# s32 | o   |   - | xxx |   - | o   |  +-
# u64 | o   |  +  |  +  | xxx |  +  |  +
# s64 | o   |  +- |  +- |   - | xxx |  +-
# bit | o   | o   | o   | o   | o   | xxx
#
# Boolean implementation of the above table:
#     | flt | u32 | s32 | u64 | s64 | bit
# ----+-----+-----+-----+-----+-----+-----
# flt | o+- |  +- |  +- |  +- |  +- | x+-
# u32 | o   | xxx |  +  | o   | o   |  +
# s32 | o   |   - | x+x |   - | o   |  +-
# u64 | o   |  +  |  +  | x+x |  +  |  +
# s64 | o   |  +- |  +- |   - | xx- |  +-
# bit | o   | o   | o   | o   | o   | o+x
#

# Enable (val > MAX) test
MAXEN="s,@MAXEN@,$([[ \
		"$1" == "float" || \
		"$2" == "bit" || \
		( "$2" == "s32" && "$1" != "bit" ) || \
		( "$1" == "u64" && "$2" != "float" ) || \
		( "$1" == "s64" && "$2" == "u32" ) \
	]]; echo $?),g"

# Enable (val < MIN) test
MINEN="s,@MINEN@,$([[ \
		"$1" == "float" || \
		( "$1" == "s64" && "$2" != "float" ) || \
		( "$1" == "s32" && ( "$2" == "u32" || "$2" == "u64" || "$2" == "bit" ) ) \
	]]; echo $?),g"

# Disable clamp code
CC="s,@CC@,$([[ \
		"$2" == "float" || \
		"$1" == "bit" || \
		( "$1" == "u32" && ( "$2" == "u64" || "$2" == "s64" ) ) || \
		( "$1" == "s32" && "$2" == "s64" ) \
	]] && echo "//"),g"

FP="s,@FP@,$([[ "$1" == "float" || "$2" == "float" ]] || echo "nofp"),g"
IN="s,@IN@,$1,g"
OUT="s,@OUT@,$2,g"
MIN="s,@MIN@,$(minval "$2"),g"
MAX="s,@MAX@,$(maxval "$2"),g"
TYPI="s,@TYPI@,$(utype "$1"),g"
TYPO="s,@TYPO@,$(utype "$2"),g"

exec sed -e "$IN; $OUT; $CC; $MIN; $MAX; $FP; $TYPI; $TYPO; $MINEN; $MAXEN;"
