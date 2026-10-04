"""Import the pinned Read Pico hardware sources; exclude demo UI and SD/FatFS."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

PIN = "28cde682a4468a581c278761922724f57d976418"
ROOT = Path(__file__).resolve().parents[1] / "libs/hardware/ReadPicoHardware"
COMPONENTS = ("epdiy", "cst836u", "fca9555", "sy7636a", "read_pico_pmu", "e0470_epaper_waveform")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?")
    parser.add_argument("--check", action="store_true", help="verify the imported source manifest")
    args = parser.parse_args()
    if args.check:
        manifest = json.loads((ROOT / "vendor-manifest.json").read_text(encoding="utf-8"))
        expected = manifest["files"]
        present = {path.relative_to(ROOT / "vendor").as_posix() for path in (ROOT / "vendor").rglob("*") if path.is_file()}
        if present != set(expected):
            parser.error("vendor file list differs from manifest")
        for relative, digest in expected.items():
            if hashlib.sha256((ROOT / "vendor" / relative).read_bytes()).hexdigest() != digest:
                parser.error(f"vendor source changed: {relative}")
        print(f"Verified {len(expected)} vendor files at {manifest['commit']}")
        return
    if args.source is None:
        parser.error("provide a pinned source checkout or --check")
    source = args.source.resolve()
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        parser.error(f"source must be at {PIN}, found {head}")
    if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip():
        parser.error("source checkout must be clean")
    files = []
    for component in COMPONENTS:
        directory = source / "components" / component
        for path in sorted(directory.rglob("*")):
            rel = path.relative_to(directory)
            if not path.is_file() or any(part in ("examples", "docs") for part in rel.parts):
                continue
            if path.suffix not in (".c", ".h", ".S", ".cmake", ".yml") and path.name not in ("LICENSE", "README.md", "CMakeLists.txt"):
                continue
            files.append(path)
    board = source / "components/read_pico"
    files.extend(board / name for name in (
        "LICENSE", "read_pico_board.c", "read_pico_epd_timing.c",
        "include/read_pico_board.h", "include/read_pico_epd_timing.h",
    ))
    manifest = {"repository": "https://github.com/MindReset/read_pico_firmware", "commit": PIN, "files": {}}
    for path in files:
        rel = path.relative_to(source / "components")
        target = ROOT / "vendor" / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        manifest["files"][rel.as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    (ROOT / "vendor-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Imported {len(files)} files from {PIN}")


if __name__ == "__main__":
    main()
