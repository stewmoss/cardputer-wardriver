Import("env")
import shutil
import os
import atexit

# Read version from platformio.ini [env] custom_fw_version option
version = env.GetProjectOption("custom_fw_version", "dev")

# Inject as a C preprocessor macro so firmware code can reference FIRMWARE_VERSION
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])

# Resolve paths once at script load time
build_dir = env.subst("$BUILD_DIR")
firmware_src = os.path.join(build_dir, "firmware.bin")
output_dir = os.path.normpath(os.path.join(build_dir, "..", "..", "..", "output"))
dest = os.path.join(output_dir, f"wardriver-{version}.bin")

def copy_firmware():
    if not os.path.isfile(firmware_src):
        return
    os.makedirs(output_dir, exist_ok=True)
    shutil.copy2(firmware_src, dest)
    print(f"\n*** Firmware v{version} -> {dest}\n")

atexit.register(copy_firmware)
