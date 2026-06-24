Import("env")
import re, os, shutil, atexit

def get_active_profile(defines_path):
    try:
        with open(defines_path, "r") as f:
            content = f.read()
        match = re.search(r'^[ \t]*#[ \t]*define[ \t]+(PROFILE_\w+)', content, re.MULTILINE)
        if match:
            return match.group(1)
    except Exception as e:
        print(f"[firmware_name] Warning: could not read defines.h: {e}")
    return None

defines_h = os.path.join(env.get("PROJECT_SRC_DIR", "src"), "defines.h")
profile = get_active_profile(defines_h)

if profile:
    build_dir = env.subst("$BUILD_DIR")

    def copy_bin_at_exit():
        src = os.path.join(build_dir, "firmware.bin")
        dst = os.path.join(build_dir, f"firmware-{profile}.bin")
        if os.path.exists(src):
            shutil.copy2(src, dst)
            print(f"[firmware_name] Created: {dst}")
        else:
            print(f"[firmware_name] {src} not found, skipping")

    atexit.register(copy_bin_at_exit)
    print(f"[firmware_name] Will produce firmware-{profile}.bin after build")
else:
    print("[firmware_name] Warning: no active PROFILE_xxx found in defines.h")
