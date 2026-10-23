"""Validate runtime QAD1..7 headers without loading the full sidecar."""
import struct


def validate_qadj_header(path, route_header, route_bytes):
    with path.open('rb') as f:
        data = f.read(80)
    if len(data) < 8:
        raise ValueError('route.qadj is too small')
    magic, version, entry = struct.unpack_from('<4sHH', data)
    if version not in range(1, 8) or magic != f'QAD{version}'.encode():
        raise ValueError('route.qadj must be QAD1 through QAD7')
    size = {1: 40, 2: 48, 3: 64, 4: 64, 5: 64, 6: 80, 7: 80}[version]
    if len(data) < size or entry != (12 if version < 5 else 16):
        raise ValueError('route.qadj header/entry size mismatch')
    v = struct.unpack_from(f'<{(size-8)//4}I', data, 8)
    n, ch, uf, ur, edges, source_bytes = v[:6]
    r = route_header
    if v[:6] != (r[3], r[9], r[10], r[11], r[6], route_bytes):
        raise ValueError('route.qadj does not match this route.cpr')
    end = path.stat().st_size
    ok = False
    if version == 1:
        forward, reverse = v[6:8]
        ok = forward == size and reverse == forward + uf*12 and end == reverse + ur*12
    elif version == 2:
        index, start, count, flags = v[6:10]
        ok = index == size and start == index+(n+1)*4 and end == start+count and flags & 1
    elif version in (3, 4):
        shift, groups, index, start, count, recon, recon_bytes, flags = v[6:14]
        required = 3 if version == 3 else 7
        ok = (3 <= shift <= 8 and groups == (n+(1 << shift)-1) >> shift
              and index == size and start == index+(groups+1)*4
              and recon == start+count and recon_bytes == ch*(20 if version == 3 else 28)
              and end == recon+recon_bytes and flags & required == required)
    else:
        index, index_bytes, start, count, recon, recon_bytes = v[6:12]
        flags = v[12]
        if version == 6 and flags & 62 != 62:
            extra, extra_bytes, points, flags = v[12:16]
            extra_entry = 12
        elif version in (6, 7):
            extra, extra_bytes, points, extra_entry = v[14:18]
        required = {5: 30, 6: 62, 7: 94}[version]
        ok = (index == size and index_bytes == n*4 and start == index+index_bytes
              and recon == start+count and recon_bytes == ch*(32 if version == 7 else 28)
              and flags & required == required)
        if version == 5:
            ok = ok and end == recon+recon_bytes
        else:
            width = 20 if version == 7 else 12
            ok = (ok and extra == recon+recon_bytes and extra_entry == width
                  and extra_bytes == points*width and end == extra+extra_bytes
                  and (version == 7 or points == r[12]))
    if not ok or end > 0x7fffffff:
        raise ValueError('route.qadj has an invalid runtime layout or size')
    return version
