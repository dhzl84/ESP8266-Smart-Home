"""Move the version file to the build directory."""
import shutil
import os
Import("env")

def move_version_file(source, target, env):
    """move the version file to the build directory"""
    version_file = "version"
    version_file_target = env.subst("$BUILD_DIR") + os.sep + env.subst("version")
    if os.path.exists(version_file):
        shutil.move(version_file, version_file_target)
    else:
        print(f"version file '{version_file}' does not exist!")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", move_version_file)
