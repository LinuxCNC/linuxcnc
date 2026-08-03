"""Camera matrices recorded from the pre-refactor GL compatibility path.

References were captured with Mesa llvmpipe's OpenGL 4.5 compatibility context
using an 800x600 viewport, center (1.25, -2.5, 3.75), and extents (4, 6, 8).
OpenGL returns column-major matrices, hence the transpose before comparison.
"""

import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "lib", "python"))

import glnav


class Navigation(glnav.GlNavBase):
    def _redraw(self):
        pass

    def winfo_width(self):
        return 800

    def winfo_height(self):
        return 600

    def extents_info(self):
        return (1.25, -2.5, 3.75), (4.0, 6.0, 8.0)

    def is_lathe(self):
        return self.lathe


def legacy_matrix(columns):
    return np.asarray(columns, dtype=np.float64).T


REFERENCES = {
    "rotate_snap": [[1, 0, 0, 0], [0, -4.3711388e-08, 1, 0],
                    [0, -1, -4.3711388e-08, 0], [0, 1.25, 6.25, 1]],
    "translate_zoom_boost": [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0],
                             [0.1273418665, 0.0955063999, 0, 1]],
    "view_x": [[-4.3711388e-08, 0, 1, 0], [1, -4.3711388e-08, 4.3711388e-08, 0],
               [4.3711388e-08, 1, 1.9106855e-15, 0], [2.49999976, -3.75, -1.24999988, 1]],
    "view_y": [[-4.3711388e-08, -1, 4.3711388e-08, 0], [0, -4.3711388e-08, -1, 0],
               [1, -4.3711388e-08, 1.9106855e-15, 0], [-3.75, 1.25, -2.5, 1]],
    "view_y2": [[-4.3711388e-08, 1, 4.3711388e-08, 0], [0, -4.3711388e-08, 1, 0],
                [1, 4.3711388e-08, 1.9106855e-15, 0], [-3.75, -1.25000024, 2.5, 1]],
    "view_z": [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [-1.25, 2.5, -3.75, 1]],
    "view_z2": [[-4.3711388e-08, -1, 0, 0], [1, -4.3711388e-08, 0, 0],
                [0, 0, 1, 0], [2.5, 1.24999988, -3.75, 1]],
    "view_p": [[.9063076973, -.2113092095, .3659983277, 0],
               [.4226184487, .4531538188, -.7848855257, 0],
               [0, .8660254478, .4999999702, 0],
               [-.0763385296, -1.850574255, -4.29471159, 1]],
}


class ExplicitCameraMatrixTest(unittest.TestCase):
    def nav(self, lathe=False):
        result = Navigation()
        result.lathe = lathe
        return result

    def assert_matrix(self, name, nav):
        np.testing.assert_allclose(nav.get_modelview_matrix(), legacy_matrix(REFERENCES[name]),
                                   rtol=0, atol=3e-6)

    def test_rotation_snaps_to_ninety_degrees(self):
        nav = self.nav()
        nav.set_centerpoint(1.25, -2.5, 3.75)
        nav.lat = 89
        nav.recordMouse(0, 0)
        nav.rotate(4, 2)  # 90° and 2° both snap to legacy 90°/0° angles.
        self.assert_matrix("rotate_snap", nav)

    def test_translate_uses_projection_and_zoom_boost(self):
        nav = self.nav()
        nav.distance = 2.0
        nav.recordMouse(10, 20)
        nav.translate(30, 5)
        self.assert_matrix("translate_zoom_boost", nav)

    def test_zoom_changes_distance_but_not_modelview(self):
        nav = self.nav()
        original = nav.get_modelview_matrix()
        nav.zoomin()
        nav.zoomout()
        np.testing.assert_allclose(nav.get_modelview_matrix(), original)
        self.assertAlmostEqual(nav.distance, 9.98)

    def test_view_presets_match_legacy(self):
        for name, lathe in (("x", False), ("y", True), ("y2", False),
                            ("z", False), ("z2", False), ("p", False)):
            with self.subTest(view=name):
                nav = self.nav(lathe)
                getattr(nav, "set_view_" + name)()
                self.assert_matrix("view_" + name, nav)

    def test_translate_modelview_matches_manual_composition(self):
        nav = self.nav()
        nav.set_view_p()
        before = nav.get_modelview_matrix()
        nav.translate_modelview(3.5, -1.75, 0.25)
        expected = glnav.multiply(before, glnav.translation_matrix(3.5, -1.75, 0.25))
        np.testing.assert_allclose(nav.get_modelview_matrix(), expected)

    def test_translate_modelview_offset_survives_plasmac_sequence(self):
        # The plasmac table view: top view, offset to the machine table centre,
        # then refit the eyepoint.  The offset has to survive the refit.
        xcenter, ycenter = 12.0, -7.5
        nav = self.nav()
        nav.set_view_z()
        after_preset = nav.get_modelview_matrix()
        nav.translate_modelview(-xcenter, -ycenter, 0)
        nav.set_eyepoint_from_extents(4.0, 6.0)
        expected = glnav.multiply(after_preset,
                                  glnav.translation_matrix(-xcenter, -ycenter, 0))
        np.testing.assert_allclose(nav.get_modelview_matrix(), expected)

    def test_translate_modelview_leaves_navigation_state_alone(self):
        nav = self.nav()
        nav.set_view_p()
        nav.recordMouse(10, 20)
        nav.translate(30, 5)
        state = (nav.lat, nav.lon, nav.distance, nav._totalx, nav._totaly)
        nav.translate_modelview(-4.0, 2.5, 0)
        self.assertEqual((nav.lat, nav.lon, nav.distance, nav._totalx, nav._totaly), state)

    def test_ortho_projection_matches_legacy_scale(self):
        nav = self.nav()
        nav.distance = 2.0
        k = 2.0 ** .55555
        expected = glnav.multiply(
            glnav.ortho_matrix(-k, k, -k * 600 / 800, k * 600 / 800, -1000, 1000),
            glnav.translation_matrix(0, 0, -1))
        np.testing.assert_allclose(nav.get_projection_matrix(800, 600), expected)


if __name__ == "__main__":
    unittest.main()
