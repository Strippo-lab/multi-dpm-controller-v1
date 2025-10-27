import subprocess
print("🐍 get_git_versio.py running...")
def get_git_version():
    try:
        # Use last Git tag, or commit hash if no tag
        version = (
            subprocess.check_output(["git", "describe", "--tags", "--always"], stderr=subprocess.DEVNULL)
            .decode()
            .strip()
        )
        # Optional: strip 'v' prefix if you like (v1.4.3 -> 1.4.3)
        if version.startswith("v"):
            version = version[1:]
        return version
    except Exception:
        return "dev"
