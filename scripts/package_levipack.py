#!/usr/bin/env python3
"""Pack libBedrockVisuals.so + manifest + config into a .levipack (zip)."""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import zipfile
from pathlib import Path


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--library", required=True, type=Path)
    p.add_argument("--manifest", required=True, type=Path)
    p.add_argument("--config", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()

    if not args.library.is_file():
        print(f"library not found: {args.library}", file=sys.stderr)
        return 1
    if not args.manifest.is_file():
        print(f"manifest not found: {args.manifest}", file=sys.stderr)
        return 1

    mod_id = "bedrockvisuals"
    try:
        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        # folder name inside zip = stable mod id
        mod_id = "bedrockvisuals"
    except Exception:
        pass

    out = args.output.resolve()
    if out.exists():
        out.unlink()

    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        # root folder = mod id
        zf.write(args.library, arcname=f"{mod_id}/{args.library.name}")
        zf.write(args.manifest, arcname=f"{mod_id}/manifest.json")
        if args.config.is_file():
            zf.write(args.config, arcname=f"{mod_id}/config/config.json")

    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
