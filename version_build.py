Import("env")
import os

# Version aus version.txt lesen (Increment passiert im Post-Build nach erfolgreichem Build)
version_file = os.path.join(env.subst("$PROJECT_DIR"), "src", "version.txt")
try:
    with open(version_file, 'r') as f:
        version = f.read().strip()
except:
    version = "dev"

print(f"Building version: {version}")

# FW_VERSION als build_flag hinzufügen
flag = '-DFW_VERSION=\\"' + version + '\\"'
env.Append(BUILD_FLAGS=flag)
