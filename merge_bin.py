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

def merge_bin(source, target, env):
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
        f'--chip esp32 merge_bin '
        f'-o {q(merged)} '
        f'--flash_mode dio '
        f'--flash_freq 40m '
        f'--flash_size 4MB '
        f'0x1000 {q(bootloader)} '
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

    firmware_versioned = os.path.join(bins_dir, f"firmware-v{version}.bin")
    print(f"Copying {firmware} -> {firmware_versioned}")
    shutil.copy2(firmware, firmware_versioned)

    merged_versioned = os.path.join(bins_dir, f"display-reader-universal-v{version}.bin")
    print(f"Copying {merged} -> {merged_versioned}")
    shutil.copy2(merged, merged_versioned)

    # --- docs-Ordner: Binaries fuer Web-Installer und OTA ---
    docs_dir = os.path.join(project_dir, "docs")
    os.makedirs(docs_dir, exist_ok=True)

    # Merged Binary fuer Web-Installer (esptool, offset 0x0)
    docs_firmware = os.path.join(docs_dir, "firmware.bin")
    print(f"Copying merged -> {docs_firmware}")
    shutil.copy2(merged, docs_firmware)

    # App-only Binary fuer OTA-Update (Upload ueber Web-UI)
    docs_ota = os.path.join(docs_dir, "firmware-ota.bin")
    print(f"Copying firmware (OTA) -> {docs_ota}")
    shutil.copy2(firmware, docs_ota)

    # --- docs/manifest.json: Version aktualisieren ---
    manifest_path = os.path.join(docs_dir, "manifest.json")
    try:
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
        manifest["version"] = version
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
            f.write('\n')
        print(f"manifest.json updated to version {version}")
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
