#!/usr/bin/env python3

"""
    This file contains helper methods for icon theme related stuff.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""
import os

from gi.repository import GObject
from gi.repository import Gtk


def find_valid_icon_themes(search_paths):
    valid_icon_themes = [
        # path, name
    ]
    for base_dir in [e for e in search_paths if os.path.exists(e)]:
        for theme_name in sorted(os.listdir(base_dir)):
            theme_dir = os.path.join(base_dir, theme_name)
            if os.path.exists(os.path.join(theme_dir, "index.theme")):
                # theme_dir acts here as unique key for the model and is not used as a dir later
                valid_icon_themes.append((theme_dir, theme_name))
    return valid_icon_themes


def find_handler_id_by_signal(obj, signal_name):
    signal_id, detail = GObject.signal_parse_name(signal_name, obj, True)
    return GObject.signal_handler_find(obj, GObject.SignalMatchType.ID, signal_id, detail, None, None, None)


def load_symbolic_from_icon_theme(icon_theme, icon_name, size, style=None):
    """Load a symbolic icon from the current icon theme.
    If style is given, the symolic icon will be recolored based on colors derive from the stylecontext:
     foreground color from Gtk.StateFlags.NORMAL, success_color, warning_color and error_color by calling
     the corresponding lookup_color method.
    If style is None, the icon is loaded via the Gtk.IconInfo.load_icon method without recoloring.

    :param icon_name: The icon name
    :type icon_name: str
    :param size: Icon size to load
    :type size: int
    :param style: The style context to derive the colors from
    :type style: Gtk.StyleContext
    :return: GdkPixbuf.Pixbuf
    :raises: ValueError: if icon lookup fails (usually if the theme does not contain a icon with this name)
    """
    lookup_flags = Gtk.IconLookupFlags.USE_BUILTIN | Gtk.IconLookupFlags.FORCE_SYMBOLIC | Gtk.IconLookupFlags.FORCE_SIZE
    icon_info = icon_theme.lookup_icon(icon_name, size, lookup_flags)
    if icon_info is None:
        raise ValueError(f"Lookup icon '{icon_name}' failed")

    pixbuf = None
    if style is not None:
        fg = style.get_color(Gtk.StateFlags.NORMAL)
        __, success_color = style.lookup_color("success_color")
        __, warning_color = style.lookup_color("warning_color")
        __, error_color = style.lookup_color("error_color")

        pixbuf, _ = icon_info.load_symbolic(fg, success_color, warning_color, error_color)
    else:
        pixbuf = icon_info.load_icon()

    return pixbuf
