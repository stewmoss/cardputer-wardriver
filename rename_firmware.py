Import("env")
import shutil
import os
import atexit
import subprocess
from pathlib import Path

# Read version from platformio.ini [env] custom_fw_version option
version = env.GetProjectOption("custom_fw_version", "dev")

# Inject as a C preprocessor macro
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])

# Resolve paths
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
    
def generate_hash():    
    # This is the hash generation powerhsell script
    script_path = Path(r".\get-check-sha256.ps1")
    
    if not script_path.is_file():
        print(f"Error: The script was not found at {script_path.absolute()}")
        return None

    try:
        # Run the script
        # check=True: Raises CalledProcessError if return code != 0
        result = subprocess.run(
            ["pwsh.exe", "-ExecutionPolicy", "Bypass", "-File", str(script_path), "-filePath", dest],
            capture_output=True,
            text=True,
            check=True 
        )
        
        print(f"Hash Generated Successfully Stdout:\n{result.stdout}")
        return result.stdout

    except FileNotFoundError:
        # This triggers if 'pwsh.exe' is not found in the system PATH
        print("Error: 'pwsh.exe' (PowerShell Core) was not found. Please ensure it is installed and in your PATH.")
    except subprocess.CalledProcessError as e:
        # This triggers if the script itself returns an error code
        print(f"Error: The PowerShell script failed with exit code {e.returncode}")
        print(f"Details: {e.stderr}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

atexit.register(generate_hash)
atexit.register(copy_firmware)
