Import("env")
import shutil
import os

# Read version from platformio.ini [env] custom_fw_version option
version = env.GetProjectOption("custom_fw_version", "dev")

# Inject as a C preprocessor macro so firmware code can reference FIRMWARE_VERSION
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])

def copy_firmware(source, target, env):
    
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")

    output_dir = os.path.normpath(os.path.join(build_dir, "..", "..", "..", "output"))
    print(f"*** Output directory: {output_dir}\n")
    output_dir_name = output_dir
    os.makedirs(output_dir, exist_ok=True)

    dest = os.path.join(output_dir_name, f"deauthdetector-{version}.bin")
    print (f"*** Post Build Action: Version={version}\n")
    print (f"*** Post Build Action: Copy firmware.bin to '{dest}'\n")
    print(f"*** Output directory: {output_dir_name}\n") 

    if os.path.isfile(src):
        shutil.copy2(src, dest)
        print(f"\n*** Output: {dest}\n")
    else:
        print(f"\n*** source not found: {src}\n")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
