#!/usr/bin/env python3
# generate.py — creates test data files for inflate/zipreader tests
# Run from the repository root or the tests/data directory.
# Requires Python 3.6+ (zlib, zipfile, struct, json).

import zlib, zipfile, struct, json, io, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def write(name, data):
    path = os.path.join(HERE, name)
    with open(path, 'wb') as f:
        f.write(data)
    print(f'  wrote {path} ({len(data)} bytes)')


def main():
    print('Generating inflate/zipreader test data ...')

    # --- test.zip -----------------------------------------------------------------
    # Three entries: stored.txt (stored), deflated.txt (deflate, dynamic Huffman),
    # manifest.txt (stored).
    json_data = json.dumps(
        [{'key': f'value{i}', 'num': i, 'flag': i % 2 == 0} for i in range(100)]
    ).encode()

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w') as z:
        zi = zipfile.ZipInfo('stored.txt')
        zi.compress_type = zipfile.ZIP_STORED
        z.writestr(zi, 'Hello, stored world!\n')

        zi2 = zipfile.ZipInfo('deflated.txt')
        zi2.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(zi2, json_data)

        zi3 = zipfile.ZipInfo('manifest.txt')
        zi3.compress_type = zipfile.ZIP_STORED
        z.writestr(zi3, 'version=2026.12.0\nbuild=sdcard\n')

    zip_data = buf.getvalue()
    write('test.zip', zip_data)

    # Verify
    buf2 = io.BytesIO(zip_data)
    with zipfile.ZipFile(buf2) as z:
        assert z.read('stored.txt') == b'Hello, stored world!\n'
        assert z.read('deflated.txt') == json_data
        assert z.read('manifest.txt') == b'version=2026.12.0\nbuild=sdcard\n'
    print('  test.zip verified OK')

    # --- corrupt.zip: flip a bit in the CRC of stored.txt -------------------------
    cd_start = zip_data.find(b'PK\x01\x02')
    assert cd_start >= 0, 'central directory not found'
    crc_offset = cd_start + 16  # CRC field in first CD entry
    corrupt = bytearray(zip_data)
    corrupt[crc_offset] ^= 0xFF
    write('corrupt.zip', bytes(corrupt))

    # --- truncated.zip: cut in half -----------------------------------------------
    write('truncated.zip', zip_data[:len(zip_data) // 2])

    print('Done.')


if __name__ == '__main__':
    main()
