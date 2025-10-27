# rename_firmware.py
# -------------------------------------------------------------------
# Safe post-build rename script for PlatformIO ESP32 projects
# Waits for firmware.bin, renames with version & timestamp,
# archives to firmware_builds and optional export dir.
# -------------------------------------------------------------------

Import("env")
import os, time, shutil
from datetime import datetime

print("🐍 rename_firmware.py running...")

# -------------------------------------------------------------------
# Extract FW_VERSION_STRING from build flags
# -------------------------------------------------------------------
def get_version_from_flags():
    for flag in env.get("BUILD_FLAGS", []):
        if "FW_VERSION_STRING" in flag:
            val = flag.split("=")[1]
            val = val.replace('"', "").replace("'", "").replace("\\", "").strip()
            return val
    return "vUnknown"

# -------------------------------------------------------------------
# Main rename logic
# -------------------------------------------------------------------
def rename_after_build(source, target, env):
    print("🐍 rename_firmware.py post-build hook triggered")

    build_dir = env.subst("$BUILD_DIR")
    firmware_path = os.path.join(build_dir, "firmware.bin")

    # 🕐 Wait up to 10s for firmware.bin to appear
    for i in range(100):
        if os.path.exists(firmware_path):
            break
        time.sleep(0.1)
    else:
        print(f"⚠️ Firmware not found after waiting: {firmware_path}")
        return

    fw_version = get_version_from_flags()
    timestamp = datetime.now().strftime("%Y%m%d_%H%M")
    new_name = f"firmware_{fw_version}_{timestamp}.bin"
    new_path = os.path.join(build_dir, new_name)

    try:
        # Copy renamed file inside build dir
        shutil.copy2(firmware_path, new_path)
        print(f"✅ Copied as: {new_name}")

        # --- Copy to firmware_builds archive ---
        archive_dir = os.path.join(env["PROJECT_DIR"], "firmware_builds")
        os.makedirs(archive_dir, exist_ok=True)
        shutil.copy2(new_path, os.path.join(archive_dir, new_name))
        print(f"📦 Archived to: {archive_dir}")

        # --- Optional external export directory ---
        export_dir = env.GetProjectOption("custom_firmware_export_dir", None)
        if export_dir:
            os.makedirs(export_dir, exist_ok=True)
            shutil.copy2(new_path, os.path.join(export_dir, new_name))
            print(f"🌍 Exported to: {export_dir}")
        else:
            print("ℹ️ No external export directory defined")

        # --- Clean up ---
        try:
            os.remove(firmware_path)
            print(f"🧹 Deleted original: {firmware_path}")
        except Exception as e:
            print(f"⚠️ Could not delete original: {e}")

    except Exception as e:
        print(f"❌ Rename failed: {e}")

# -------------------------------------------------------------------
# Hook: run after final binary creation
# -------------------------------------------------------------------
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", rename_after_build)
