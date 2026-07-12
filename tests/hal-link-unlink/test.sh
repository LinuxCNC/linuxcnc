#!/bin/bash
# hal_lib pin link/unlink value-preservation semantics.
#
# Classic drove this via the Python `hal` module, which created a userspace
# component with its own pins.  gomc has no userspace components (an external
# process cannot create HAL pins), so we load a stock comp — tristate_float,
# which has a float IN pin `in` and a float IO pin `out` — into a resident
# server and drive the identical sequence with halcmd.  Verifies, per pin:
#   1. linking a pin to a *virgin* signal adopts the pin's value (not zero)
#   2. writing through the link updates the pin
#   3. unlinking retains the last value (inherited from the signal)
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server link-unlink.hal || exit 1

fail=0
getp() { halcmd getp "$1" | awk '{print $NF}'; }
check() {  # <pin> <expected> <label>
    local got; got=$(getp "$1")
    if [ -z "$got" ] || ! awk "BEGIN{exit !($got==$2)}" 2>/dev/null; then
        echo "FAIL: $3: $1 = '$got', expected $2"; fail=1
    fi
}

for pin in tristate_float.in tristate_float.out; do
    sig="sig_${pin#tristate_float.}"
    halcmd setp "$pin" 4712 >/dev/null
    check "$pin" 4712 "setp on unlinked pin"

    halcmd newsig "$sig" float >/dev/null
    halcmd net "$sig" "$pin" >/dev/null
    check "$pin" 4712 "link to virgin signal keeps pin value"

    halcmd sets "$sig" 815 >/dev/null
    check "$pin" 815 "write through link"

    halcmd unlinkp "$pin" >/dev/null
    check "$pin" 815 "unlink retains last value"
done

[ $fail -eq 0 ] && echo OK
exit $fail
