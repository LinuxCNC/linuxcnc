#!/usr/bin/env python3
"""
ADS/AMS PCAP parser for debugging hal-ads-server traffic.

Parses tcpdump/tcpreplay pcap files and outputs human-readable text suitable
for diff comparison between a working and broken capture.

Usage:
    python3 src/hal/hal-ads-server/pcap-parser.py capture.pcap > capture.txt
    python3 src/hal/hal-ads-server/pcap-parser.py working.pcap > working.txt
    python3 src/hal/hal-ads-server/pcap-parser.py broken.pcap > broken.txt
    diff -u working.txt broken.txt
"""

import sys
import struct
import hashlib
from collections import defaultdict

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ADS_PORT = 48898

# Link types
LINKTYPE_ETHERNET = 1
LINKTYPE_LINUX_SLL2 = 276

# ADS Command IDs
CMD_READ_DEVICE_INFO = 0x01
CMD_READ = 0x02
CMD_WRITE = 0x03
CMD_READ_STATE = 0x04
CMD_READ_WRITE = 0x09

COMMAND_NAMES = {
    CMD_READ_DEVICE_INFO: "ReadDeviceInfo",
    CMD_READ: "Read",
    CMD_WRITE: "Write",
    CMD_READ_STATE: "ReadState",
    CMD_READ_WRITE: "ReadWrite",
}

# StateFlags
STATE_RESPONSE = 0x0001
STATE_ADS_CMD = 0x0004

# Index Group names
INDEX_GROUP_NAMES = {
    0xF003: "SymbolHandleByName",
    0xF005: "SymbolValueByHandle",
    0xF006: "SymbolReleaseHandle",
    0xF007: "SymbolInfoByName",
    0xF080: "SumRead",
    0xF081: "SumWrite",
    0x4040: "ProcessImageRW",
}

# ADS Error codes
ERROR_CODES = {
    0x0000: "NoError",
    0x0701: "ServiceNotSupported",
    0x070D: "NotFound",
    0x0710: "InvalidSymbol",
    0x1869: "ClientInvalidHdl",
}

# ---------------------------------------------------------------------------
# pcap / packet parsing
# ---------------------------------------------------------------------------

PCAP_GLOBAL_HEADER_SIZE = 24
PCAP_RECORD_HEADER_SIZE = 16


def parse_pcap(data):
    """Yield (ts_sec, ts_usec, packet_data) for each packet in a pcap file."""
    if len(data) < PCAP_GLOBAL_HEADER_SIZE:
        return

    magic = struct.unpack_from("<I", data, 0)[0]
    if magic == 0xA1B2C3D4:
        endian = "<"
    elif magic == 0xD4C3B2A1:
        endian = ">"
    else:
        raise ValueError(f"Not a valid pcap file (magic=0x{magic:08X})")

    _ver_maj, _ver_min, _thiszone, _sigfigs, _snaplen, network = struct.unpack_from(
        endian + "HHiIII", data, 4
    )

    offset = PCAP_GLOBAL_HEADER_SIZE
    while offset + PCAP_RECORD_HEADER_SIZE <= len(data):
        ts_sec, ts_usec, incl_len, _orig_len = struct.unpack_from(
            endian + "IIII", data, offset
        )
        offset += PCAP_RECORD_HEADER_SIZE
        if offset + incl_len > len(data):
            break
        pkt = data[offset : offset + incl_len]
        offset += incl_len
        yield ts_sec, ts_usec, pkt, network


def parse_ethernet(pkt):
    """Return (ip_payload, proto) from an Ethernet frame, or (None, None)."""
    if len(pkt) < 14:
        return None, None
    etype = struct.unpack_from(">H", pkt, 12)[0]
    if etype == 0x8100:  # VLAN
        if len(pkt) < 18:
            return None, None
        etype = struct.unpack_from(">H", pkt, 16)[0]
        return pkt[18:], etype
    return pkt[14:], etype


def parse_linux_sll2(pkt):
    """
    Parse a LINUX_SLL2 (link type 276) header (20 bytes).
    Returns (ip_payload, proto_type) or (None, None).
    """
    if len(pkt) < 20:
        return None, None
    proto_type = struct.unpack_from(">H", pkt, 0)[0]
    return pkt[20:], proto_type


def parse_ip(data):
    """
    Parse an IPv4 header.
    Returns (src_ip, dst_ip, proto, payload) or (None,)*4 on failure.
    """
    if not data or len(data) < 20:
        return None, None, None, None
    version_ihl = data[0]
    version = version_ihl >> 4
    if version != 4:
        return None, None, None, None
    ihl = (version_ihl & 0x0F) * 4
    if ihl < 20 or len(data) < ihl:
        return None, None, None, None
    proto = data[9]
    src_ip = data[12:16]
    dst_ip = data[16:20]
    return src_ip, dst_ip, proto, data[ihl:]


def parse_tcp(data):
    """
    Parse a TCP header.
    Returns (src_port, dst_port, seq, ack, flags, payload) or (None,)*6.
    """
    if not data or len(data) < 20:
        return None, None, None, None, None, None
    src_port, dst_port, seq, ack = struct.unpack_from(">HHII", data, 0)
    data_offset = (data[12] >> 4) * 4
    if data_offset < 20 or len(data) < data_offset:
        return None, None, None, None, None, None
    flags = data[13] & 0x3F
    payload = data[data_offset:]
    return src_port, dst_port, seq, ack, flags, payload


# ---------------------------------------------------------------------------
# AMS / ADS protocol parsing
# ---------------------------------------------------------------------------

AMS_TCP_HEADER_SIZE = 6   # reserved(2) + length(4)
AMS_HEADER_SIZE = 32      # full AMS header without AMS/TCP wrapper


def parse_ams_header(data):
    """
    Parse a 32-byte AMS header.
    Returns a dict with all fields, or None if too short.
    """
    if len(data) < AMS_HEADER_SIZE:
        return None
    target_netid = data[0:6]
    target_port = struct.unpack_from("<H", data, 6)[0]
    src_netid = data[8:14]
    src_port = struct.unpack_from("<H", data, 14)[0]
    cmd_id, state_flags, length, error_code, invoke_id = struct.unpack_from(
        "<HHIII", data, 16
    )
    return {
        "target_netid": target_netid,
        "target_port": target_port,
        "src_netid": src_netid,
        "src_port": src_port,
        "cmd_id": cmd_id,
        "state_flags": state_flags,
        "length": length,
        "error_code": error_code,
        "invoke_id": invoke_id,
    }


def error_name(code):
    name = ERROR_CODES.get(code)
    if name:
        return f"{name}(0x{code:04X})"
    return f"0x{code:08X}"


def ig_name(ig):
    name = INDEX_GROUP_NAMES.get(ig)
    if name:
        return f"{name}(0x{ig:04X})"
    return f"0x{ig:08X}"


def hex_dump(data):
    return " ".join(f"{b:02X}" for b in data)


def int_repr(data):
    """Show integer interpretation for 1/2/4/8 byte data."""
    n = len(data)
    if n == 1:
        return f" (uint8={data[0]})"
    if n == 2:
        return f" (uint16={struct.unpack_from('<H', data)[0]})"
    if n == 4:
        return f" (uint32={struct.unpack_from('<I', data)[0]})"
    if n == 8:
        return f" (uint64={struct.unpack_from('<Q', data)[0]})"
    return ""


def decode_ads_data(cmd_id, state_flags, ig, ads_data):
    """
    Decode ADS payload and return a list of display lines (strings).
    """
    lines = []
    is_response = bool(state_flags & STATE_RESPONSE)

    if cmd_id == CMD_READ_DEVICE_INFO:
        if not is_response:
            pass  # no ADS data in request
        else:
            if len(ads_data) >= 8:
                result, major, minor, build = struct.unpack_from("<IBBH", ads_data)
                device_name = ads_data[8:24].rstrip(b"\x00").decode("ascii", errors="replace")
                lines.append(
                    f"     Result={error_name(result)} MajorVer={major} "
                    f"MinorVer={minor} BuildVer={build} DeviceName={device_name!r}"
                )

    elif cmd_id == CMD_READ:
        if not is_response:
            pass  # IG/IO/Length already on header line
        else:
            if len(ads_data) >= 8:
                _result, length = struct.unpack_from("<II", ads_data)
                rdata = ads_data[8 : 8 + length]
                if rdata:
                    lines.append(f"     Data: {hex_dump(rdata)}{int_repr(rdata)}")

    elif cmd_id == CMD_WRITE:
        if not is_response:
            if len(ads_data) >= 12:
                ig_val, io_val, length = struct.unpack_from("<III", ads_data)
                wdata = ads_data[12 : 12 + length]
                if wdata:
                    lines.append(f"     WriteData: {hex_dump(wdata)}{int_repr(wdata)}")
        else:
            pass  # Result already on header line

    elif cmd_id == CMD_READ_STATE:
        if not is_response:
            pass  # no ADS data in request
        else:
            if len(ads_data) >= 8:
                result, ads_state, dev_state = struct.unpack_from("<IHH", ads_data)
                lines.append(
                    f"     Result={error_name(result)} ADSState={ads_state} DeviceState={dev_state}"
                )

    elif cmd_id == CMD_READ_WRITE:
        if not is_response:
            if len(ads_data) >= 16:
                ig_val, io_val, read_len, write_len = struct.unpack_from("<IIII", ads_data)
                wdata = ads_data[16 : 16 + write_len]
                # Smart display for write data
                if ig_val in (0xF003, 0xF007):
                    sym = wdata.rstrip(b"\x00").decode("ascii", errors="replace")
                    wdata_str = f'"{sym}"'
                elif ig_val == 0xF006 and len(wdata) == 4:
                    handle = struct.unpack_from("<I", wdata)[0]
                    wdata_str = f"Handle=0x{handle:08X}"
                elif ig_val == 0xF080 and len(wdata) % 12 == 0:
                    entries = []
                    for i in range(0, len(wdata), 12):
                        e_ig, e_io, e_sz = struct.unpack_from("<III", wdata, i)
                        entries.append(
                            f"[{ig_name(e_ig)} IO=0x{e_io:08X} Len={e_sz}]"
                        )
                    wdata_str = " ".join(entries)
                else:
                    wdata_str = hex_dump(wdata) + int_repr(wdata) if wdata else "(empty)"
                if wdata or ig_val in (0xF003, 0xF007, 0xF006):
                    lines.append(f"     WriteData: {wdata_str}")
        else:
            if len(ads_data) >= 8:
                result, read_len = struct.unpack_from("<II", ads_data)
                rdata = ads_data[8 : 8 + read_len]
                # Smart display for read data
                if rdata:
                    if len(rdata) == 12:
                        r_ig, r_io, r_sz = struct.unpack_from("<III", rdata)
                        lines.append(
                            f"     ReadData: [compact] IG={ig_name(r_ig)} "
                            f"IO=0x{r_io:08X} Size={r_sz}"
                        )
                    elif len(rdata) == 4 and ig == 0xF003:
                        handle = struct.unpack_from("<I", rdata)[0]
                        lines.append(f"     ReadData: Handle=0x{handle:08X}")
                    else:
                        lines.append(
                            f"     ReadData: {hex_dump(rdata)}{int_repr(rdata)}"
                        )
    else:
        # Unknown command: dump raw data
        if ads_data:
            lines.append(f"     RawData: {hex_dump(ads_data)}")

    return lines


def format_header_line(num, hdr, ads_data):
    """Format the primary summary line for an ADS message."""
    is_response = bool(hdr["state_flags"] & STATE_RESPONSE)
    direction = "RSP" if is_response else "REQ"
    cmd_id = hdr["cmd_id"]
    cmd_name = COMMAND_NAMES.get(cmd_id, f"Cmd0x{cmd_id:04X}")
    invoke = hdr["invoke_id"]

    parts = [f"#{num:03d} {direction}  InvokeID=0x{invoke:04X} {cmd_name}"]

    # Inline fields per command type on the summary line
    if cmd_id == CMD_READ_WRITE:
        if not is_response:
            if len(ads_data) >= 16:
                ig_val, io_val, read_len, write_len = struct.unpack_from("<IIII", ads_data)
                parts.append(
                    f" IG={ig_name(ig_val)} IO=0x{io_val:08X}"
                    f" ReadLen={read_len} WriteLen={write_len}"
                )
        else:
            if len(ads_data) >= 8:
                result, read_len = struct.unpack_from("<II", ads_data)
                parts.append(f" Result={error_name(result)} ReadLen={read_len}")
    elif cmd_id == CMD_READ:
        if not is_response:
            if len(ads_data) >= 12:
                ig_val, io_val, length = struct.unpack_from("<III", ads_data)
                parts.append(f" IG={ig_name(ig_val)} IO=0x{io_val:08X} Length={length}")
        else:
            if len(ads_data) >= 4:
                result = struct.unpack_from("<I", ads_data)[0]
                parts.append(f" Result={error_name(result)}")
    elif cmd_id == CMD_WRITE:
        if not is_response:
            if len(ads_data) >= 12:
                ig_val, io_val, length = struct.unpack_from("<III", ads_data)
                parts.append(f" IG={ig_name(ig_val)} IO=0x{io_val:08X} Length={length}")
        else:
            if len(ads_data) >= 4:
                result = struct.unpack_from("<I", ads_data)[0]
                parts.append(f" Result={error_name(result)}")

    return "".join(parts)


# ---------------------------------------------------------------------------
# Main processing
# ---------------------------------------------------------------------------

def process_pcap(filename):
    """
    Parse a pcap file and return a list of formatted text lines representing
    all ADS messages found.
    """
    with open(filename, "rb") as f:
        data = f.read()

    # Deduplication set: (seq, ack, payload_hash)
    seen = set()

    output_lines = []
    msg_counter = [0]

    def process_stream_buffer(buf, invoke_ig):
        """Extract and decode all ADS messages from a TCP stream buffer."""
        offset = 0
        while offset + AMS_TCP_HEADER_SIZE + AMS_HEADER_SIZE <= len(buf):
            _reserved, ams_length = struct.unpack_from("<HI", buf, offset)
            total_needed = AMS_TCP_HEADER_SIZE + ams_length
            if offset + total_needed > len(buf):
                break  # need more data

            ams_block = buf[offset + AMS_TCP_HEADER_SIZE : offset + total_needed]
            hdr = parse_ams_header(ams_block)
            if hdr is None:
                offset += 1
                continue

            ads_data = ams_block[AMS_HEADER_SIZE:]

            cmd_id = hdr["cmd_id"]
            state_flags = hdr["state_flags"]
            invoke_id = hdr["invoke_id"]
            is_response = bool(state_flags & STATE_RESPONSE)

            # Track IG from requests for matching with responses
            if not is_response and cmd_id in (CMD_READ, CMD_WRITE, CMD_READ_WRITE):
                if len(ads_data) >= 4:
                    ig = struct.unpack_from("<I", ads_data)[0]
                    invoke_ig[invoke_id] = ig

            # Get IG for response context
            ig = invoke_ig.get(invoke_id, 0)

            msg_counter[0] += 1
            num = msg_counter[0]

            header_line = format_header_line(num, hdr, ads_data)
            detail_lines = decode_ads_data(cmd_id, state_flags, ig, ads_data)

            output_lines.append(header_line)
            output_lines.extend(detail_lines)

            offset += total_needed

        return buf[offset:]

    # Stream reassembly state per direction; invoke_ig shared per connection
    stream_buffers = defaultdict(bytes)
    # Canonical connection key: frozenset of the two endpoints
    invoke_ig_per_conn = defaultdict(dict)

    for ts_sec, ts_usec, pkt, network in parse_pcap(data):
        # Parse link layer
        if network == LINKTYPE_ETHERNET:
            ip_data, etype = parse_ethernet(pkt)
            if etype != 0x0800:
                continue
        elif network == LINKTYPE_LINUX_SLL2:
            ip_data, etype = parse_linux_sll2(pkt)
            if etype != 0x0800:
                continue
        else:
            continue

        # Parse IP
        src_ip, dst_ip, proto, tcp_data = parse_ip(ip_data)
        if proto != 6:  # TCP only
            continue

        # Parse TCP
        src_port, dst_port, seq, ack, flags, payload = parse_tcp(tcp_data)
        if src_port is None:
            continue

        # Filter ADS port
        if src_port != ADS_PORT and dst_port != ADS_PORT:
            continue

        # Deduplication
        if payload:
            ph = hashlib.md5(payload, usedforsecurity=False).digest()
        else:
            ph = b""
        dedup_key = (seq, ack, ph)
        if dedup_key in seen:
            continue
        seen.add(dedup_key)

        if not payload:
            continue

        # TCP stream key (per direction)
        stream_key = (src_ip, src_port, dst_ip, dst_port)

        # Connection key (shared between both directions)
        conn_key = frozenset([(src_ip, src_port), (dst_ip, dst_port)])

        # Append to stream buffer and process
        stream_buffers[stream_key] += payload
        remaining = process_stream_buffer(
            stream_buffers[stream_key],
            invoke_ig_per_conn[conn_key],
        )
        stream_buffers[stream_key] = remaining

    return output_lines


def main():
    if len(sys.argv) < 2:
        print(
            "Usage: ads_pcap_parser.py <file.pcap> [file2.pcap ...]",
            file=sys.stderr,
        )
        sys.exit(1)

    for pcap_file in sys.argv[1:]:
        if len(sys.argv) > 2:
            print(f"=== {pcap_file} ===")
        try:
            lines = process_pcap(pcap_file)
            for line in lines:
                print(line)
        except Exception as exc:
            print(f"Error processing {pcap_file}: {exc}", file=sys.stderr)


if __name__ == "__main__":
    main()

