"""Copy the campaign mods into an asset directory, converting RARs to ZIPs.

Requires 7-Zip for RAR inputs. Original archives are never modified.
"""
import argparse
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import zipfile


MODS = [
    ("barrins-folly", "Barrin's Folly", "Barrins Folly.rar"),
    ("jamuraa", "Jamuraa", "jamuraa.rar"),
    ("otaria", "Otaria (Edit 2)", "Otaria Edit2.rar"),
    ("alliances", "Alliances (Edit 7)", "Shandalar Alliances Edit 7.rar"),
    ("masques-onslaught", "Masques / Invasion / Odyssey / Onslaught",
     "Shandalar Masques Invasion Odyssey Onslaught.zip"),
]

# Summarized from Here_is_Shandalar_2026_Update.docx's campaign entries.
DESCRIPTIONS = {
    "barrins-folly": "Explore the Urza and Mercadian Masques era with rebuilt enemy decks, new art and world text, and characters including Barrin, Ertai, Jhoira, Multani, and Cho-Manno. Created by FortuneShoddy359 / Rich_Watercress7447.",
    "jamuraa": "A Mirage and Tempest campaign set in Jamuraa, with 55 rebuilt enemy decks, new art, rewritten NPCs and locations, and revised castle and dungeon effects. Created by FortuneShoddy359 / Rich_Watercress7447 as a sequel in the Mirage/Weatherlight era.",
    "otaria": "An Invasion, Odyssey, and Onslaught campaign set in Dominaria and Otaria. Jhoira recruits you into a world with Cabal and Aphetto flavor, 55 new enemy decks, rewritten text, and portraits styled as contender posters. Created by FortuneShoddy359 / Rich_Watercress7447.",
    "alliances": "Expand the original campaign through Alliances with missing early cards and all 55 enemy decks rebuilt. Cards the AI handles poorly are excluded, preserving a feel close to the original game. Created by FortuneShoddy359 / Rich_Watercress7447.",
    "masques-onslaught": "Play across the Masques, Invasion, Odyssey, and Onslaught blocks with all 55 enemy decks rebuilt. Decks draw on Standard-era archetypes such as Crosis Control, R/G Madness, and Counter-Rebels, softened for campaign play. Created by Marky_Marky_Mark.",
}


def validate_name(name):
    path = PurePosixPath(name.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or ":" in name:
        raise ValueError(f"Unsafe archive path: {name}")


def rar_entries(source, seven_zip):
    result = subprocess.run([seven_zip, "l", "-slt", "-ba", str(source)],
                            check=True, capture_output=True, text=True,
                            encoding="utf-8")
    entries = []
    for record in result.stdout.strip().split("\n\n"):
        fields = dict(line.split(" = ", 1) for line in record.splitlines()
                      if " = " in line)
        name = fields.get("Path")
        if not name:
            continue
        validate_name(name)
        if fields.get("Folder") != "+" and not fields.get("Attributes", "").startswith("D"):
            entries.append((name.replace("\\", "/"), int(fields["Size"])))
    return entries


def package(source_dir, destination, seven_zip, scratch):
    destination.mkdir(parents=True, exist_ok=True)
    manifest = {"version": 1, "mods": []}
    for mod_id, name, original in MODS:
        source = source_dir / original
        target = destination / f"{mod_id}.zip"
        if target.exists():
            raise FileExistsError(f"Refusing to overwrite {target}")
        if source.suffix.lower() == ".zip":
            with zipfile.ZipFile(source) as archive:
                for entry in archive.infolist():
                    validate_name(entry.filename)
                if archive.testzip() is not None:
                    raise ValueError(f"Corrupt ZIP: {source}")
            shutil.copy2(source, target)
        else:
            entries = rar_entries(source, seven_zip)
            with tempfile.TemporaryDirectory(prefix=f"{mod_id}-", dir=scratch) as temp:
                subprocess.run([seven_zip, "x", "-y", "-bsp0", f"-o{temp}", str(source)],
                               check=True, stdout=subprocess.DEVNULL)
                with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
                    for relative, size in entries:
                        file = Path(temp) / relative
                        if file.stat().st_size != size:
                            raise ValueError(f"Wrong extracted size: {relative}")
                        archive.write(file, relative)
                with zipfile.ZipFile(target) as archive:
                    assert archive.testzip() is None
                    assert {e.filename: e.file_size for e in archive.infolist()} == dict(entries)
                    for relative, _ in entries:
                        assert archive.read(relative) == (Path(temp) / relative).read_bytes()
        with zipfile.ZipFile(target) as archive:
            files = sum(not entry.is_dir() for entry in archive.infolist())
        manifest["mods"].append({"id": mod_id, "name": name,
                                 "description": DESCRIPTIONS[mod_id],
                                 "file": target.name, "bytes": target.stat().st_size})
        print(f"{mod_id}: verified {files} files ({target.stat().st_size:,} bytes)", flush=True)
    manifest_path = destination / "mods.json"
    if manifest_path.exists():
        raise FileExistsError(f"Refusing to overwrite {manifest_path}")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--seven-zip", default="C:/Program Files/7-Zip/7z.exe")
    parser.add_argument("--scratch", type=Path, default=Path("out/mod-packaging"))
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)
    package(args.source, args.destination, args.seven_zip, args.scratch)
