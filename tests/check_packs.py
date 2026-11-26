#!/usr/bin/env python3
"""Validate the shipped cursor assets, including every animation frame."""
from array import array
import json
import configparser
from pathlib import Path
import struct
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
packs = json.loads((root / 'src/packs.json').read_text())

def compositor_shape_source(theme, shape):
    """Reproduce Hyprland 0.56's inherited-directory precedence for installed packs."""
    pending, visited, directories = [theme], set(), {}
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        path = root / 'build/themes' / name
        if not path.exists():
            path = Path('/usr/share/icons') / name
        if (path / 'cursors').is_dir():
            directories[str(Path('/usr/share/icons') / name / 'cursors')] = (name, path / 'cursors')
        index = configparser.ConfigParser()
        index.read(path / 'index.theme')
        if index.has_section('Icon Theme'):
            pending.extend(n.strip() for n in index['Icon Theme'].get('Inherits', '').split(',') if n.strip())
    for virtual_path in sorted(directories):
        name, directory = directories[virtual_path]
        if (directory / shape).is_file():
            return name
    return None

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
    for shape in ('left_ptr', 'hand2', 'xterm', 'crosshair', 'ew-resize', 'watch'):
        actual_source = compositor_shape_source(theme, shape)
        assert actual_source == theme, f'{theme}/{shape} is overridden by {actual_source}'
    if 'archive' not in pack:
        index = configparser.ConfigParser()
        index.read(path / 'index.theme')
        native = index['Icon Theme'].getint('X-Cursora-DefaultSize')
        for cursor in (path / 'cursors').iterdir():
            if cursor.is_symlink():
                assert cursor.is_file() and '/' not in str(cursor.readlink()), f'Broken alias: {cursor}'
                continue
            images = frames(cursor.read_bytes())
            assert {meta[0] for meta, _ in images} == {native, native * 2, native * 3, native * 4}
            for meta, pixels in images:
                nominal, _, width, height, xhot, yhot, delay = meta
                assert nominal == width == height and 0 <= xhot < width and 0 <= yhot < height
                assert any(pixel >> 24 for pixel in pixels), f'Empty image: {cursor}'
                if cursor.name == 'left_ptr':
                    alpha = [p >> 24 for p in pixels]
                    assert alpha[yhot * width + xhot] > 0, f'Invisible click tip at {nominal}px'
                    assert any(0 < a < 255 for a in alpha), f'Jagged arrow edge at {nominal}px'
                    assert pixels.count(0xff0f0f0f) >= nominal * nominal // 10, f'Arrow core is too small at {nominal}px'
                else:
                    assert set(pixels) <= {0x00000000, 0xff0f0f0f, 0xfff7f7f7}, f'Unexpected color in {cursor} at {nominal}px'
            if cursor.name in ('watch', 'progress'):
                animation = [(meta[-1], pixels.tobytes()) for meta, pixels in images if meta[0] == native]
                assert len(animation) == 4 and len(set(animation)) == 4 and all(delay > 0 for delay, _ in animation)
            checked_files += 1
            checked_frames += len(images)
        continue
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
silhouettes = {}
for theme, pack in packs.items():
    family = pack['family']
    if family in silhouettes:
        continue
    path = root / 'build/themes' / theme / 'cursors/left_ptr'
    values = frames(path.read_bytes())
    meta, pixels = min(values, key=lambda f: abs(f[0][0] - 48))
    silhouettes[family] = (meta[:4], bytes(p >> 24 for p in pixels))
assert len(set(silhouettes.values())) == len(silhouettes), 'Cursor families do not differ in shape'
for archive in archives.values():
    archive.close()
print(f'Validated {len(packs)} packs, {checked_files} cursor files, {checked_frames} frames: monochrome; sizes, hotspots, timings and alpha retained.')
