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

    def svg(name):
        return Path(BASE_PATH).joinpath("svg", name)

    # ref
    ref_icons_svg = svg("ref_icons.inkscape.svg")
    composer.add("ref_all.symbolic.png",
                 src_file=ref_icons_svg,
                 context="actions",
                 layers=["origin", "all-arrows"]
                 )
    composer.add("unref_all.symbolic.png",
                 src_file=ref_icons_svg,
                 context="actions",
                 layers=["unref-all"]
                 )
    for axis in ["x", "y", "z", "a", "b", "c", "u", "v", "w"] + [str(i) for i in range(8)]:
        composer.add(f"ref_{axis}.symbolic.png",
                     src_file=ref_icons_svg,
                     context="actions",
                     layers=["origin", "up-right-arrow", axis]
                     )

    # translate
    translate_icons_svg = svg("translate_icons.inkscape.svg")
    for axis in ["x", "y", "z", "a", "b", "c", "u", "v", "w"]:
        composer.add(f"translate_{axis}.symbolic.png",
                     src_file=translate_icons_svg,
                     context="actions",
                     layers=["background", "question", axis]
                     )

    # power button & main switch
    power_icons_svg = svg("power_icons.inkscape.svg")
    main_switch_icons_svg = svg("main_switch_icons.inkscape.svg")
    for state in ["on", "off"]:
        composer.add(f"power_{state}.symbolic.png",
                     src_file=power_icons_svg,
                     context="actions",
                     layers=["background", state]
                     )
        composer.add(f"main_switch_{state}.symbolic.png",
                     src_file=main_switch_icons_svg,
                     context="actions",
                     layers=[state]
                     )

    # mode buttons
    mode_icons_svg = svg("mode_icons.inkscape.svg")
    for active in ["active", "inactive"]:
        for mode in ["manual", "mdi", "auto", "settings", "user_tabs"]:
            composer.add(f"mode_{mode}_{active}.symbolic.png",
                         src_file=mode_icons_svg,
                         context="actions",
                         layers=[active, mode]
                         )

    # back to app
    back_to_app_svg = svg("back_to_app.inkscape.svg")
    composer.add("back_to_app.symbolic.png",
                 src_file=back_to_app_svg,
                 context="actions",
                 layers=["base"]
                 )

    # logout
    logout_svg = svg("logout.inkscape.svg")
    composer.add("logout.symbolic.png",
                 src_file=logout_svg,
                 context="actions",
                 layers=["base"]
                 )

    # fullscreen
    fullscreen_icons_svg = svg("fullscreen_icons.inkscape.svg")
    for state in ["open", "close"]:
        composer.add(f"fullscreen_{state}.symbolic.png",
                     src_file=fullscreen_icons_svg,
                     context="actions",
                     layers=[state]
                     )

    # gremlin controls
    gremlin_control_icons_svg = svg("gremlin_control_icons.inkscape.svg")
    composer.add("toolpath.symbolic.png",
                 src_file=gremlin_control_icons_svg,
                 context="actions",
                 layers=["toolpath"]
                 )
    composer.add("clear.symbolic.png",
                 src_file=gremlin_control_icons_svg,
                 context="actions",
                 layers=["clear"]
                 )
    composer.add("dimensions.symbolic.png",
                 src_file=gremlin_control_icons_svg,
                 context="actions",
                 layers=["dimensions"]
                 )

    for zoom in ["zoom_in", "zoom_out"]:
        composer.add(f"{zoom}.symbolic.png",
                     src_file=gremlin_control_icons_svg,
                     context="actions",
                     layers=[zoom]
                     )

    composer.add(f"tool_axis_p.symbolic.png",
                 src_file=gremlin_control_icons_svg,
                 context="actions",
                 layers=["perspective_plane"]
                 )

    for axis in ["x", "y", "z"]:
        composer.add(f"tool_axis_{axis}.symbolic.png",
                     src_file=gremlin_control_icons_svg,
                     context="actions",
                     layers=["simple_plane", axis]
                     )

    # coolant controls
    coolant_icons_svg = svg("coolant_icons.inkscape.svg")
    for status in ["active", "inactive"]:
        for coolant in ["mist", "flood"]:
            composer.add(f"coolant_{coolant}_{status}.symbolic.png",
                         src_file=coolant_icons_svg,
                         context="actions",
                         layers=[f"{coolant}_{status}"]
                         )

    # spindle control icons
    spindle_icons_svg = svg("spindle_icons.inkscape.svg")
    for direction in ["left", "right"]:
        composer.add(f"spindle_{direction}.symbolic.png",
                     src_file=spindle_icons_svg,
                     context="actions",
                     layers=["spindle", "turn", direction]
                     )
        composer.add(f"spindle_{direction}_on.symbolic.png",
                     src_file=spindle_icons_svg,
                     context="actions",
                     layers=["spindle_on", "turn", direction]
                     )

    # spindle stop
    composer.add(f"spindle_stop_on.symbolic.png",
                 src_file=spindle_icons_svg,
                 context="actions",
                 layers=["spindle", "stop_on"]
                 )
    composer.add(f"spindle_stop.symbolic.png",
                 src_file=spindle_icons_svg,
                 context="actions",
                 layers=["spindle", "stop"]
                 )

    # jog speed
    jog_speed_svg = svg("jog_speed_icons.inkscape.svg")
    for speed in ["slow", "fast"]:
        composer.add(f"jog_speed_{speed}.symbolic.png",
                     src_file=jog_speed_svg,
                     context="actions",
                     layers=[speed]
                     )

    # chevron
    chevron_icons_svg = svg("chevron_icons.inkscape.svg")
    for direction in ["left", "right", "up"]:
        composer.add(f"chevron_{direction}.symbolic.png",
                     src_file=chevron_icons_svg,
                     context="actions",
                     layers=[direction]
                     )

    # auto menu icons
    auto_icons_svg = svg("auto_icons.inkscape.svg")
    for name in ["open_file", "refresh", "play", "stop", "pause", "pause_active", "step", "run_from_line",
                 "skip_optional_active", "skip_optional_inactive", "edit_code"]:
        composer.add(f"{name}.symbolic.png",
                     src_file=auto_icons_svg,
                     context="actions",
                     layers=[name.replace("_", "-")]
                     )

    # folder icons
    folder_icons_svg = svg("folder_icons.inkscape.svg")
    for name in ["home_folder", "user_defined_folder"]:
        composer.add(f"{name}.symbolic.png",
                     src_file=folder_icons_svg,
                     context="actions",
                     layers=[name.replace("_", "-")]
                     )

    # edit icons
    edit_icons_svg = svg("edit_icons.inkscape.svg")
    for name in ["save", "save_as", "new_document", "keyboard", "keyboard_hide"]:
        composer.add(f"{name}.symbolic.png",
                     src_file=edit_icons_svg,
                     context="actions",
                     layers=[name.replace("_", "-")]
                     )

    # compose all images
    composer.compose(DIMENSIONS)


def create_theme_index():
    """Create theme index file"""

    os.makedirs(TARGET_DIR, exist_ok=True)

    index = "index.theme"
    src = Path(BASE_PATH).joinpath(index)
    dest = copyfile(src, Path(TARGET_DIR).joinpath(index))
    if dest:
        print(f"copied {src} -> {dest}")


def copy_license_files():
    """Copy license files"""

    os.makedirs(TARGET_DIR, exist_ok=True)

    for file in ["LICENSE", "NOTICE"]:
        src = Path(BASE_PATH).joinpath(file)
        dest = copyfile(src, Path(TARGET_DIR).joinpath(file))
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
            copy_license_files()
