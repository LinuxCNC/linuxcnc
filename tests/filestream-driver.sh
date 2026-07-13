# Shared driver for file-backed streaming HAL tests (the `filestream` cmod).
#
# Replaces the WebSocket hal-stream-driver.sh: instead of a resident server plus
# halstreamer/halsampler WebSocket clients (with a connect-before-start race and a
# fixed settle sleep), the filestream cmod owns the file I/O on its own non-RT
# thread and signals completion on its `filestream.done` pin.  A test just writes
# the replay input file, calls fs_run, and diffs the captured output file.
#
# Usage in a test.sh:
#     . "$(dirname "$0")/../filestream-driver.sh"    # adjust ../ depth
#     printf '...\n' > in.txt        # replay input (matches halstreamer format)
#     fs_run <halfile> [outfile]     # default outfile: out.txt
#
# The .hal must `load filestream ... samples=N` (so `done` fires after N captured
# samples) and NOT call `start` (the driver owns thread lifecycle).

_FS_SRVPID=""

fs_cleanup() {
    [ -n "$_FS_SRVPID" ] && kill "$_FS_SRVPID" 2>/dev/null
    wait 2>/dev/null
}

# fs_run <halfile> [outfile]: start a resident gomc-server on <halfile>, run the
# HAL threads until filestream signals done, stop, and print <outfile> (the
# captured samples) to stdout as the test result.
fs_run() {
    local halfile=$1 outfile=${2:-out.txt}
    rm -f "$outfile" server.log
    gomc-server -r -f "$halfile" --serve >server.log 2>&1 &
    _FS_SRVPID=$!
    trap fs_cleanup EXIT

    local i
    for i in $(seq 100); do
        halcmd show comp 2>/dev/null | grep -q filestream && break
        sleep 0.1
    done

    halcmd start
    # Wait for the run to complete (replay drained / N samples captured).
    for i in $(seq 500); do
        [ "$(halcmd getp filestream.done 2>/dev/null | awk '{print $NF}')" = TRUE ] && break
        sleep 0.02
    done
    halcmd stop

    # Kill the server so filestream's Destroy flushes the final captures and
    # closes the output file before we read it.
    kill "$_FS_SRVPID" 2>/dev/null
    wait "$_FS_SRVPID" 2>/dev/null
    _FS_SRVPID=""

    cat "$outfile"
}
