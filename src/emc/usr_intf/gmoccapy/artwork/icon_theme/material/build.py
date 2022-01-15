import sys
import os
from shutil import rmtree, copyfile
from pathlib import Path
from _build_helper import Composer, Dimension

BASE_PATH = Path(__file__).parent.absolute()
TEMP_DIR = Path(BASE_PATH).joinpath("temp")
TARGET_DIR = Path(BASE_PATH).joinpath("../../../../../../../share/gmoccapy/icons/material").resolve()

DIMENSIONS = list(Dimension(s) for s in [16, 24, 32, 48])


def clean(dirs):
    """Delete each directory"""
    for dir in dirs:
        if dir.exists():
            rmtree(dir)
            print(f"deleted {dir}")


def compose_images():
    """Compose images"""

    composer = Composer(TARGET_DIR, TEMP_DIR)

def create_theme_index():
    """Create theme index file"""

    os.makedirs(TARGET_DIR, exist_ok=True)

    index = "index.theme"
    src = Path(BASE_PATH).joinpath(index)
    dest = copyfile(src, Path(TARGET_DIR).joinpath(index) )
    if dest:
        print(f"copied {src} -> {dest}")

def copy_license_files():
    """Copy license files"""

    os.makedirs(TARGET_DIR, exist_ok=True)

    for file in ["LICENSE", "NOTICE"]:
        src = Path(BASE_PATH).joinpath(file)
        dest = copyfile(src, Path(TARGET_DIR).joinpath(file) )
        if dest:
            print(f"copied {src} -> {dest}")

def build():
    """Default build steps"""

    clean([TARGET_DIR, TEMP_DIR])
    compose_images()
    clean([TEMP_DIR])
    create_theme_index()
    copy_license_files()


if __name__ == '__main__':
    if len(sys.argv) == 1:
        build()
    else:
        if "clean" in sys.argv:
            clean([TARGET_DIR, TEMP_DIR])

        if "compose" in sys.argv:
            compose_images()

        if "index" in sys.argv:
            create_theme_index()

        if "license" in sys.argv:
            create_theme_index()
