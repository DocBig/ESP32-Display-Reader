Import("env")
import os
import re
import shutil
import json

def q(path):
    return '"' + path + '"'

def read_version(version_file):
    try:
        with open(version_file, 'r') as f:
            return f.read().strip()
    except:
        return "unknown"

def increment_version(version):
    """Inkrementiert die letzten 3 Stellen: 1.0.000 -> 1.0.001, 1.0.099 -> 1.0.100"""
    m = re.match(r'^(\d+\.\d+\.)(\d+)$', version)
    if m:
        prefix = m.group(1)
        build_num = int(m.group(2))
        return prefix + f"{build_num + 1:03d}"
    return version

# Pro-Env Merge-Parameter. universal_esp32 bleibt unveraendert (bestehende
# docs/*.bin und bin's/*.bin Pfade), neue Boards bekommen eigene chip-/
# flash-Parameter und einen eigenen docs/<env>/-Unterordner, damit sich
# verschiedene Envs beim Bauen nicht gegenseitig ueberschreiben.
ENV_CONFIGS = {
    "universal_esp32": {
        "chip": "esp32",
        "bootloader_offset": "0x1000",
        "flash_mode": "dio",
        "flash_freq": "40m",
        "flash_size": "4MB",
        "chip_family": "ESP32",
        "docs_subdir": None,          # docs/ root (bestehendes Verhalten)
        "bins_suffix": "",            # firmware-v1.0.000.bin (bestehendes Verhalten)
        "manifest_part_path": "firmware.bin",  # relativ zu docs/manifest.json
    },
    "atoms3r_m12": {
        "chip": "esp32s3",
        "bootloader_offset": "0x0",
        # flash_mode/freq fuer S3 mit dio_opi memory_type (Flash=DIO, PSRAM=OPI).
        # DIO statt QIO: QIO haengt vom Quad-Enable-Statusbit des Flash-Chips
        # ab, das auf diesem Modul nicht zuverlaessig gesetzt ist -> ROM-
        # Bootloader haengt sich nach "ets_loader.c 78" auf, Watchdog-Reset,
        # Boot-Loop. Verifiziert gegen `pio run -e atoms3r_m12 -v` (PlatformIO's
        # eigener esptool-Merge-Aufruf nutzt exakt diese Werte).
        "flash_mode": "dio",
        "flash_freq": "80m",
        "flash_size": "8MB",
        "chip_family": "ESP32-S3",
        "docs_subdir": "atoms3r_m12",
        "bins_suffix": "-atoms3r_m12",
        "manifest_part_path": "atoms3r_m12/firmware.bin",  # relativ zu docs/manifest.json
    },
}

# Alle Boards teilen sich EIN manifest.json (docs/manifest.json) mit einem
# "builds"-Eintrag pro chipFamily. ESP Web Tools erkennt beim Verbinden den
# tatsaechlich angeschlossenen Chip automatisch und waehlt daraus den
# passenden Build - ein einziger Install-Button fuer alle Boards.
SHARED_MANIFEST_SUBPATH = "manifest.json"

def merge_bin(source, target, env):
    pioenv = env.subst("$PIOENV")
    cfg = ENV_CONFIGS.get(pioenv)
    if cfg is None:
        print(f"merge_bin.py: no ENV_CONFIGS entry for env '{pioenv}', skipping merge.")
        return

    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    merged = os.path.join(build_dir, "display-reader-universal.bin")

    python_exe = env.subst("$PYTHONEXE")
    esptool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    esptool_py = os.path.join(esptool_dir, "esptool.py")

    cmd = (
        f'{q(python_exe)} {q(esptool_py)} '
        f'--chip {cfg["chip"]} merge_bin '
        f'-o {q(merged)} '
        f'--flash_mode {cfg["flash_mode"]} '
        f'--flash_freq {cfg["flash_freq"]} '
        f'--flash_size {cfg["flash_size"]} '
        f'{cfg["bootloader_offset"]} {q(bootloader)} '
        f'0x8000 {q(partitions)} '
        f'0x10000 {q(firmware)}'
    )

    print("Merging firmware image...")
    print(cmd)

    result = env.Execute(cmd)
    if result != 0:
        print("Merge failed")
        env.Exit(1)

    # Aktuelle Version lesen
    version_file = os.path.join(project_dir, "src", "version.txt")
    version = read_version(version_file)

    # --- bin's-Ordner: versionierte Kopien ---
    bins_dir = os.path.join(project_dir, "bin's")
    os.makedirs(bins_dir, exist_ok=True)

    suffix = cfg["bins_suffix"]
    firmware_versioned = os.path.join(bins_dir, f"firmware{suffix}-v{version}.bin")
    print(f"Copying {firmware} -> {firmware_versioned}")
    shutil.copy2(firmware, firmware_versioned)

    merged_versioned = os.path.join(bins_dir, f"display-reader-universal{suffix}-v{version}.bin")
    print(f"Copying {merged} -> {merged_versioned}")
    shutil.copy2(merged, merged_versioned)

    # --- docs-Ordner: Binaries fuer Web-Installer und OTA ---
    docs_dir = os.path.join(project_dir, "docs")
    if cfg["docs_subdir"]:
        docs_dir = os.path.join(docs_dir, cfg["docs_subdir"])
    os.makedirs(docs_dir, exist_ok=True)

    # Merged Binary fuer Web-Installer (esptool, offset 0x0)
    docs_firmware = os.path.join(docs_dir, "firmware.bin")
    print(f"Copying merged -> {docs_firmware}")
    shutil.copy2(merged, docs_firmware)

    # App-only Binary fuer OTA-Update (Upload ueber Web-UI)
    docs_ota = os.path.join(docs_dir, "firmware-ota.bin")
    print(f"Copying firmware (OTA) -> {docs_ota}")
    shutil.copy2(firmware, docs_ota)

    # --- docs/manifest.json: gemeinsames Manifest fuer alle Boards, ein
    # "builds"-Eintrag pro chipFamily wird hier eingefuegt/aktualisiert. ---
    manifest_path = os.path.join(project_dir, "docs", SHARED_MANIFEST_SUBPATH)
    try:
        try:
            with open(manifest_path, 'r') as f:
                manifest = json.load(f)
        except FileNotFoundError:
            manifest = {"name": "ESP32 Display Reader", "version": version, "builds": []}
            print(f"manifest.json missing at {manifest_path}, creating new one")

        build_entry = {
            "chipFamily": cfg["chip_family"],
            "parts": [
                {"path": cfg["manifest_part_path"], "offset": 0}
            ]
        }

        builds = manifest.setdefault("builds", [])
        existing = next((b for b in builds if b.get("chipFamily") == cfg["chip_family"]), None)
        if existing is not None:
            existing.update(build_entry)
        else:
            builds.append(build_entry)

        manifest["version"] = version
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
            f.write('\n')
        print(f"manifest.json updated to version {version} (build: {cfg['chip_family']})")
    except Exception as e:
        print(f"Warning: Could not update manifest.json: {e}")

    # --- Version inkrementieren (nach erfolgreichem Build) ---
    new_version = increment_version(version)
    try:
        with open(version_file, 'w') as f:
            f.write(new_version)
        print(f"Version incremented: {version} -> {new_version}")
    except Exception as e:
        print(f"Warning: Could not write version file: {e}")

    print(f"Build {version} complete. Next build will be {new_version}.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
