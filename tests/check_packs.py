#!/usr/bin/env python3
"""Validate the shipped cursor assets, including every animation frame."""
from array import array
import json
from pathlib import Path
import struct
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
packs = json.loads((root / 'src/packs.json').read_text())

def frames(data):
    magic, header, version, count = struct.unpack_from('<4sIII', data)
    assert magic == b'Xcur', 'Invalid Xcursor file'
    result = []
    for i in range(count):
        kind, nominal, offset = struct.unpack_from('<III', data, header + i * 12)
        if kind != 0xfffd0002:
            continue
        chunk = struct.unpack_from('<9I', data, offset)
        width, height = chunk[4:6]
        pixels = array('I', data[offset + chunk[0]:offset + chunk[0] + width * height * 4])
        if sys.byteorder != 'little':
            pixels.byteswap()
        result.append((chunk[2:], pixels))
    assert result, 'Missing image frames'
    return result

archives = {}
checked_files = checked_frames = 0
for theme, pack in packs.items():
    path = root / 'build/themes' / theme
    assert (path / 'COPYING').is_file(), f'Missing license: {theme}'
    if pack['archive'] not in archives:
        archives[pack['archive']] = tarfile.open(root / 'vendor' / pack['archive'])
    archive = archives[pack['archive']]
    prefix = pack['member'] + '/cursors/'
    for entry in archive.getmembers():
        normalized = entry.name.removeprefix('./')
        if not normalized.startswith(prefix):
            continue
        output = path / 'cursors' / normalized[len(prefix):]
        if entry.issym():
            assert output.is_symlink() and output.exists(), f'Broken alias: {output}'
            continue
        if not entry.isfile():
            continue
        original = frames(archive.extractfile(entry).read())
        actual = frames(output.read_bytes())
        assert len(original) == len(actual), f'Frame count changed: {output}'
        for (old_meta, old_pixels), (meta, pixels) in zip(original, actual):
            assert old_meta == meta, f'Size, hotspot or timing changed: {output}'
            for before, after in zip(old_pixels, pixels):
                assert before >> 24 == after >> 24, f'Alpha changed: {output}'
                if after >> 24:
                    red, green, blue = (after >> 16) & 255, (after >> 8) & 255, after & 255
                    assert red == green == blue, f'Colored pixel: {output}'
            checked_frames += 1
        checked_files += 1

# Distinct source packs must also produce distinct pointer silhouettes.
silhouettes = []
for family in ('Bibata', 'BreezeX', 'Phinger'):
    path = root / 'build/themes' / f'Cursora-{family}-Black/cursors/left_ptr'
    values = frames(path.read_bytes())
    meta, pixels = min(values, key=lambda f: abs(f[0][0] - 48))
    silhouettes.append((meta[:4], bytes(p >> 24 for p in pixels)))
assert len(set(silhouettes)) == 3, 'Cursor families do not differ in shape'
for archive in archives.values():
    archive.close()
print(f'Validated {len(packs)} packs, {checked_files} cursor files, {checked_frames} frames: monochrome; sizes, hotspots, timings and alpha retained.')
