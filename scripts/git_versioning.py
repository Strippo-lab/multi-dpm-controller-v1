print("🐍 git_versioning.py running...")
Import("env")
from get_git_version import get_git_version

version = get_git_version()
env.Append(BUILD_FLAGS=[f'-DFW_VERSION_STRING=\\"v{version}\\"'])
print(f"🔖 Firmware version from Git: v{version}")
