#!/usr/bin/env python3
"""Download pinned Entware binaries for offline tests, without installing them."""

import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--install', action='store_true', help='Download the complete pinned entware-opt fixture')
    args = parser.parse_args()
    destination = args.directory.absolute()
    if destination.exists() or destination.is_symlink():
        parser.exit(1, 'Directory already exists; left unchanged\n')
    fixture = 'entware-install-fixture.json' if args.install else 'entware-fixture.json'
    manifest = json.loads((Path(__file__).parent / 'tests' / fixture).read_text())
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix='.entware-test-') as temporary:
        stage = Path(temporary) / 'packages'
        stage.mkdir()
        for name, expected in manifest['files'].items():
            path = Path(name)
            if path.is_absolute() or '..' in path.parts:
                parser.exit(1, 'Invalid fixture path\n')
            with urllib.request.urlopen(manifest['repository'] + name, timeout=30) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != expected:
                parser.exit(1, 'Checksum mismatch: ' + name + '\n')
            target = stage / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        stage.rename(destination)
    print('Entware test packages: ' + str(destination))


if __name__ == '__main__':
    main()
