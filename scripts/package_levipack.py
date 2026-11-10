#!/usr/bin/env python3
"""Pack libCustomSkies.so + manifest + config + assets into a .levipack (zip)."""
from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--library", required=True, type=Path)
    p.add_argument("--manifest", required=True, type=Path)
    p.add_argument("--config", required=True, type=Path)
    p.add_argument("--assets", type=Path, default=Path("assets"))
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()

    if not args.library.is_file():
        print(f"library not found: {args.library}", file=sys.stderr)
        return 1
    if not args.manifest.is_file():
        print(f"manifest not found: {args.manifest}", file=sys.stderr)
        return 1

    mod_id = "customskies"
    try:
        data = json.loads(args.manifest.read_text(encoding="utf-8"))
        # keep stable id
    except Exception:
        pass

    out = args.output.resolve()
    if out.exists():
        out.unlink()

    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        # root folder = mod id (LeviLaunchroid convention)
        zf.write(args.library, arcname=f"{mod_id}/{args.library.name}")
        zf.write(args.manifest, arcname=f"{mod_id}/manifest.json")
        if args.config.is_file():
            zf.write(args.config, arcname=f"{mod_id}/config/config.json")

        # bundle the video
        if args.assets.is_dir():
            for f in args.assets.rglob("*"):
                if f.is_file():
                    arc = f"{mod_id}/assets/{f.relative_to(args.assets).as_posix()}"
                    zf.write(f, arcname=arc)
                    print(f"  + {arc}")

    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
