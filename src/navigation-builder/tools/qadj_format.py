"""Validate the current QAD7 sidecar header without reading the whole file."""
import struct


def validate_qadj_header(path, route_header, route_bytes):
    with path.open('rb') as f:
        data = f.read(80)
    if len(data) != 80:
        raise ValueError('route.qadj has an invalid header size')
    magic, version, entry, *v = struct.unpack('<4sHH18I', data)
    if (magic, version, entry) != (b'QAD7', 7, 16):
        raise ValueError('route.qadj must be QAD7')
    n, ch, uf, ur, edges, source_bytes = v[:6]
    r = route_header
    if r[:2] != (b'CPR2', 5) or tuple(v[:6]) != (r[3], r[9], r[10], r[11], r[6], route_bytes):
        raise ValueError('route.qadj does not match this CPR2 v5 route.cpr')
    index, index_bytes, start, count, recon, recon_bytes, flags, reserved, packet, packet_bytes, points, width = v[6:]
    end = path.stat().st_size
    if not (index == 80 and index_bytes == n*4 and start == index+index_bytes
            and recon == start+count and recon_bytes == ch*32 and flags & 94 == 94
            and packet == recon+recon_bytes and width == 20 and packet_bytes == points*width
            and end == packet+packet_bytes and end <= 0x7fffffff):
        raise ValueError('route.qadj has an invalid runtime layout or size')
    return version
