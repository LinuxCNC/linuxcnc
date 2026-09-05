"""The parts built from a finished program record: markers, solids, palettes.

``rs274.glcanon_bake`` turns an adopted :class:`ProgramGeometry` into the
buffers the GL renderer draws - the trajectory, the dwell markers, the
per-plane palettes, the Lambert-shaded tool solids. This checks those, on
records built by hand (``canon.FakePreview``) so a test can ask for a
*particular* four-vertex program or eleven dwells in eleven colours.

The GEOMETRY-string transform those records' positions came from is no longer
Python's: it is the C renderer's, and it is checked against
``line9_reference`` - the independent scalar transcription in this directory -
on a real parse, in ``test_transform.py``.

GL-free: needs numpy alone, and runs on a tree with no built extension.
"""

import importlib.util
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))


def _load_bake():
    # Load rs274/glcanon_bake.py directly by path: importing it as
    # `rs274.glcanon_bake` would run rs274/__init__.py, which pulls in the
    # Linux-only gcode.so and breaks host test runs. The bake module itself
    # only needs numpy.
    path = os.path.join(os.path.dirname(__file__), "..", "..", "lib", "python",
                        "rs274", "glcanon_bake.py")
    spec = importlib.util.spec_from_file_location("glcanon_bake", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bake = _load_bake()

from canon import FakePreview  # noqa: E402


class ToolSolids(unittest.TestCase):
    """The Lambert-shaded meshes."""

    def test_cone_mesh(self):
        slices = 32
        mesh = bake.cone_mesh(base_radius=0.1, height=0.25, slices=slices)
        # side (slices tris) + cap (slices tris) = 2*slices triangles.
        self.assertEqual(mesh.shape, (2 * slices * 3, 6))
        # Every normal is unit length.
        normals = mesh[:, 3:6]
        np.testing.assert_allclose(np.linalg.norm(normals, axis=1),
                                   np.ones(len(normals)), atol=1e-5)
        # Cap vertices (second half) all face +z.
        cap = mesh[slices * 3:]
        np.testing.assert_allclose(cap[:, 3:6],
                                   np.tile((0, 0, 1), (len(cap), 1)), atol=1e-6)
        # Base ring sits at z = height, apexes at origin.
        self.assertAlmostEqual(float(mesh[:slices * 3, 2].max()), 0.25, places=5)
        self.assertAlmostEqual(float(mesh[:slices * 3, 2].min()), 0.0, places=5)


COLORS = {
    "traverse": (0.3, 0.5, 0.5), "traverse_alpha": 0.33,
    "traverse_xy": (0.3, 0.5, 0.5), "traverse_alpha_xy": 0.33,
    "traverse_uv": (0.3, 0.5, 0.5), "traverse_alpha_uv": 0.33,
    "straight_feed": (1.0, 1.0, 1.0), "straight_feed_alpha": 0.33,
    "straight_feed_xy": (0.2, 1.0, 0.2), "straight_feed_alpha_xy": 0.33,
    "straight_feed_uv": (0.2, 0.2, 1.0), "straight_feed_alpha_uv": 0.33,
    "arc_feed": (1.0, 1.0, 1.0), "arc_feed_alpha": 0.5,
    "arc_feed_xy": (0.2, 1.0, 0.2), "arc_feed_alpha_xy": 0.33,
    "arc_feed_uv": (0.2, 0.2, 1.0), "arc_feed_alpha_uv": 0.33,
    "dwell": (1.0, 0.5, 0.5), "m1xx": (0.5, 0.5, 1.0),
}

#: One rapid, one feed and one arc along one path: four vertices, the first a
#: record where the strip starts. The shape a three-move program has.
PATH = [(0.0, 0.0, 0.0), (5.0, 0.0, 0.0), (5.0, 5.0, 0.0), (0.0, 5.0, 0.0)]
KINDS = [bake.KIND_NOOP, bake.KIND_TRAVERSE, bake.KIND_FEED, bake.KIND_ARC]
LINES = [1, 1, 2, 3]


def filled(is_foam=False, geometry="XYZ"):
    """A record holding that path, adopted the way a parse's is."""
    planes = [np.array(PATH, dtype=np.float32)]
    if is_foam:
        # A second drawn plane, distinct from the first so a test can tell
        # them apart, and flat in Z as both foam planes are: where a plane is
        # drawn is a matrix offset, never a coordinate.
        planes.append(planes[0] * np.float32(0.5))
    g = bake.ProgramGeometry(geometry=geometry, is_foam=is_foam)
    g.adopt(FakePreview(planes, LINES, KINDS, moves=3), COLORS)
    return g


class DwellMarkers(unittest.TestCase):
    """The marker buffer built from the geometry's transformed dwell table."""

    def geometry(self, dwells, is_foam=False):
        """A record whose dwell table is exactly ``dwells``.

        The table is a public attribute of the record, and the marker buffer
        is built from it and from nothing else - which is the point: a marker
        holds a transformed position per drawn plane, and a colour the C side
        does not carry.
        """
        g = filled(is_foam=is_foam,
                   geometry="XY;UV" if is_foam else "XYZ")
        g.dwells = [(lineno, tuple(rgba), plane,
                     tuple((x, y, z) for _ in g.planes))
                    for lineno, rgba, x, y, z, plane in dwells]
        return g

    def test_two_arms_per_marker(self):
        g = self.geometry([(3, (1.0, 0.5, 0.5, 1.0), 1.0, 2.0, 0.0, 0)])
        part = bake.dwell_marker_part(g)
        self.assertEqual(part["kind"], "program_array")
        self.assertEqual(part["mode"], bake.MODE_LINES)
        self.assertEqual(len(part["attrs"]), 4)
        self.assertEqual(len(part["planes"]), 1)
        np.testing.assert_array_equal(part["attrs"]["line"], [3, 3, 3, 3])
        np.testing.assert_array_equal(part["attrs"]["kindtool"], [0, 0, 0, 0])
        self.assertEqual(part["palettes"][0][0], (1.0, 0.5, 0.5, 1.0))

    def test_the_marker_buffer_has_no_record_kinds(self):
        """Every entry of its palette is a colour, so none is taken for a
        record - unlike the trajectory, whose codes above LAST_DRAWN_KIND are
        events rather than geometry."""
        g = self.geometry([(3, (1.0, 0.5, 0.5), 1.0, 2.0, 0.0, 0)])
        part = bake.dwell_marker_part(g)
        self.assertEqual(part["last_drawn_kind"], bake.PALETTE_SIZE - 1)
        self.assertEqual(part["hide_cat"], -1)
        self.assertNotIn("dash_cat", part)

    def test_alpha_is_not_taken_from_the_colour_table(self):
        """A three-component dwell colour is opaque.

        The legacy immediate-mode path multiplied by ``colors['dwell_alpha']``;
        the baked path never has, and both colours the canon appends are
        three-tuples. Pinned so it cannot quietly start.
        """
        g = self.geometry([(3, (0.2, 0.4, 0.6), 0.0, 0.0, 0.0, 0)])
        self.assertEqual(bake.dwell_marker_part(g)["palettes"][0][0],
                         (0.2, 0.4, 0.6, 1.0))
        g = self.geometry([(3, (0.2, 0.4, 0.6, 0.25), 0.0, 0.0, 0.0, 0)])
        self.assertEqual(bake.dwell_marker_part(g)["palettes"][0][0],
                         (0.2, 0.4, 0.6, 0.25))

    def test_palette_collects_distinct_colours(self):
        """Two colours -> two entries, and each marker indexes its own."""
        dwell_c = (0.0, 1.0, 1.0)          # colors['dwell']
        m1xx_c = (1.0, 0.0, 1.0)           # colors['m1xx']
        g = self.geometry([(3, dwell_c, 0.0, 0.0, 0.0, 0),
                           (5, m1xx_c, 1.0, 0.0, 0.0, 0),
                           (7, dwell_c, 2.0, 0.0, 0.0, 0)])
        part = bake.dwell_marker_part(g)
        self.assertEqual(part["palettes"][0][:2],
                         [dwell_c + (1.0,), m1xx_c + (1.0,)])
        kinds = part["attrs"]["kindtool"] & bake.KIND_MASK
        # Four vertices per marker: dwell, m1xx, dwell again reusing entry 0.
        np.testing.assert_array_equal(kinds, [0] * 4 + [1] * 4 + [0] * 4)

    def test_more_colours_than_the_palette_holds_is_reported(self):
        """Reported, and folded onto the last entry - never wrapped onto
        another colour's, and never silently dropped."""
        dwells = [(i, (i / 32.0, 0.0, 0.0), float(i), 0.0, 0.0, 0)
                  for i in range(bake.PALETTE_SIZE + 3)]
        part = bake.dwell_marker_part(self.geometry(dwells))
        kinds = part["attrs"]["kindtool"] & bake.KIND_MASK
        self.assertEqual(int(kinds.max()), bake.PALETTE_SIZE - 1)
        self.assertEqual(len(part["attrs"]), 4 * len(dwells))

    def test_the_lathe_forces_the_xz_plane(self):
        g = self.geometry([(3, (1.0, 0.0, 0.0), 1.0, 2.0, 3.0, 0)])
        xy = bake.dwell_marker_part(g)["planes"][0]["pos"]
        xz = bake.dwell_marker_part(g, is_lathe=True)["planes"][0]["pos"]
        # In XY the second arm varies Y; in XZ it varies Z.
        self.assertNotEqual(float(xy[2][1]), float(xy[3][1]))
        self.assertNotEqual(float(xz[2][2]), float(xz[3][2]))

    def test_foam_emits_one_marker_set_per_plane(self):
        g = self.geometry([(3, (1.0, 0.0, 0.0), 1.0, 2.0, 0.0, 0)],
                          is_foam=True)
        part = bake.dwell_marker_part(g, offsets=(0.25, 1.75))
        self.assertEqual(len(part["planes"]), 2)
        # One shared attribute array for both.
        self.assertEqual(len(part["attrs"]), 4)
        # The offsets are reported for the draw to apply, not written into the
        # positions - the same rule the trajectory follows, so that a marker
        # and the path it marks cannot end up at different heights.
        self.assertEqual(tuple(part["plane_offsets"]), (0.25, 1.75))
        np.testing.assert_allclose(part["planes"][0]["pos"][:, 2], 0.0)
        np.testing.assert_allclose(part["planes"][1]["pos"][:, 2], 0.0)


class ProgramParts(unittest.TestCase):
    """The trajectory part: one draw, one attribute array, kinds per vertex."""

    def test_nonfoam_emits_one_trajectory_and_the_dwell_buffer(self):
        parts = bake.program_parts(filled(), COLORS)
        by_name = {p["name"]: p for p in parts}
        self.assertEqual(set(by_name), {"program", "dwell"})
        prog = by_name["program"]
        self.assertEqual(prog["kind"], "program_array")
        self.assertEqual(prog["mode"], bake.MODE_LINE_STRIP)
        # The dwell markers are their own buffer, in the same format.
        self.assertEqual(by_name["dwell"]["kind"], "program_array")
        self.assertEqual(by_name["dwell"]["mode"], bake.MODE_LINES)
        # Three moves along one continuous path -> four shared vertices, one
        # plane, no chain table anywhere.
        self.assertEqual(len(prog["planes"]), 1)
        self.assertEqual(len(prog["attrs"]), 4)
        self.assertNotIn("firsts", prog)
        self.assertNotIn("counts", prog)

    def test_the_program_nominates_the_rapid_and_the_record_boundary(self):
        """Hidden yes, dashed not at all.

        Rapids draw solid, as they do in the pre-change renderer: LinuxCNC
        removed GL_LINE_STIPPLE from the rapid traverse deliberately
        (f1c1209f52, "unreliable" on some drivers), and the ``glLineStipple``
        call left behind in the preview shells is never enabled. A buffer
        nominates a hidden category and nothing else; pinned here because
        nothing else in the suite would notice a dash nomination coming back.
        """
        prog = bake.program_parts(filled(), COLORS)[0]
        self.assertNotIn("dash_cat", prog)
        self.assertEqual(prog["hide_cat"], bake.KIND_TRAVERSE)
        self.assertEqual(prog["last_drawn_kind"], bake.LAST_DRAWN_KIND)

    def test_palette_matches_the_old_per_category_colours(self):
        palette = bake.program_parts(filled(), COLORS)[0]["palettes"][0]
        self.assertEqual(palette[bake.KIND_TRAVERSE],
                         bake.resolve_rgba(COLORS, "traverse"))
        self.assertEqual(palette[bake.KIND_FEED],
                         bake.resolve_rgba(COLORS, "straight_feed"))
        self.assertEqual(palette[bake.KIND_ARC],
                         bake.resolve_rgba(COLORS, "arc_feed"))
        # Alpha rides along: feed 0.33, arc 0.5 as before.
        self.assertAlmostEqual(palette[bake.KIND_FEED][3], 0.33)
        self.assertAlmostEqual(palette[bake.KIND_ARC][3], 0.5)

    def test_each_vertex_carries_its_line_and_kind(self):
        prog = bake.program_parts(filled(), COLORS)[0]
        attrs = prog["attrs"]
        self.assertEqual(list(attrs["line"]), [1, 1, 2, 3])
        self.assertEqual(list(attrs["kindtool"] & bake.KIND_MASK),
                         [bake.KIND_NOOP, bake.KIND_TRAVERSE,
                          bake.KIND_FEED, bake.KIND_ARC])

    def test_foam_shares_attributes_and_offsets_each_plane(self):
        parts = bake.program_parts(filled(is_foam=True, geometry="XY;UV"),
                                   COLORS, is_foam=True, foam_z=0.0,
                                   foam_w=1.5)
        prog = {p["name"]: p for p in parts}["program"]
        self.assertEqual(len(prog["planes"]), 2)
        # Where each plane is drawn is reported, not baked into its vertices.
        self.assertEqual(tuple(prog["plane_offsets"]), (0.0, 1.5))
        np.testing.assert_allclose(prog["planes"][1]["pos"][:, 2], 0.0,
                                   atol=1e-6)
        np.testing.assert_allclose(prog["planes"][0]["pos"][:, 2], 0.0,
                                   atol=1e-6)
        # One attribute array for both planes, not two.
        self.assertEqual(len(prog["attrs"]), len(prog["planes"][0]))
        # Each plane draws with its own palette: _xy green, _uv blue.
        np.testing.assert_allclose(
            prog["palettes"][0][bake.KIND_FEED][:3], (0.2, 1.0, 0.2),
            atol=1e-6)
        np.testing.assert_allclose(
            prog["palettes"][1][bake.KIND_FEED][:3], (0.2, 0.2, 1.0),
            atol=1e-6)

    def test_the_span_index_is_deferred_to_the_first_highlight(self):
        """Only the highlight reads it, so the upload must not build it.

        Its temporaries are the width of the whole program - measured at
        22.8 bytes per vertex - and they would peak a few statements before
        the driver is asked to allocate the buffer.
        """
        geometry = filled()
        builds = []
        real = type(geometry)._build_index
        try:
            type(geometry)._build_index = lambda self: (
                builds.append(1) or real(self))
            prog = bake.program_parts(geometry, COLORS)[0]
            self.assertEqual(builds, [], "the upload built the index")
            self.assertTrue(callable(prog["spans"]))
            first = prog["spans"]()
            self.assertEqual(len(builds), 1)
            prog["spans"]()
            self.assertEqual(len(builds), 1, "the index was built twice")
        finally:
            type(geometry)._build_index = real
        # And what it defers is the same index as before.
        for a, b in zip(first, geometry.index):
            np.testing.assert_array_equal(a, b)

    def test_the_offsets_move_neither_the_extents_nor_the_positions(self):
        """The plane offsets are reported, not applied, and must stay so.

        ``foam_z``/``foam_w`` are a rigid translation the draw's matrix
        applies; the stored positions and the drawn extents are of the
        untranslated points. The reason the offsets are safe to relocate
        into the matrix is exactly that neither ever saw them, so it is
        asserted rather than reasoned about.
        """
        geometry = filled(is_foam=True, geometry="XY;UV")
        drawn = geometry.drawn_extents.copy()
        pos = [geometry.positions(i).copy() for i in (0, 1)]
        for foam_w in (0.0, 1.5, 9.0):
            parts = bake.program_parts(geometry, COLORS, is_foam=True,
                                       foam_w=foam_w)
            np.testing.assert_array_equal(geometry.drawn_extents, drawn)
            for i in (0, 1):
                np.testing.assert_array_equal(parts[0]["planes"][i]["pos"],
                                              pos[i])

    def test_the_arrays_handed_over_are_the_canon_s_own(self):
        """The part dict references the record's arrays; it does not copy them.

        The copy this replaces was 12 bytes per vertex, allocated while the
        driver was allocating the buffer to receive it. Asserted by identity
        rather than by value, because a copy that happens to be equal is
        exactly the failure this guards: it costs the memory and passes any
        comparison.
        """
        geometry = filled()
        prog = bake.program_parts(geometry, COLORS)[0]
        self.assertTrue(np.shares_memory(prog["planes"][0],
                                         geometry.plane_array(0)))
        self.assertTrue(np.shares_memory(prog["attrs"], geometry.attrs))

    def test_the_plane_offsets_are_not_baked_into_the_record(self):
        """foam_z/foam_w say where a plane is drawn, not what the program is.

        They can still move while the program is parsing, and they are a rigid
        translation, so neither the record nor the part holds them: the same arrays serve any pair of offsets, and moving a plane
        is a matrix change rather than a re-upload.
        """
        geometry = filled(is_foam=True, geometry="XY;UV")
        before = geometry.positions(1).copy()
        near = bake.program_parts(geometry, COLORS, is_foam=True, foam_w=1.5)
        far = bake.program_parts(geometry, COLORS, is_foam=True, foam_w=9.0)
        np.testing.assert_array_equal(geometry.positions(1), before)
        np.testing.assert_array_equal(near[0]["planes"][1], far[0]["planes"][1])
        self.assertEqual(tuple(near[0]["plane_offsets"]), (0.0, 1.5))
        self.assertEqual(tuple(far[0]["plane_offsets"]), (0.0, 9.0))


if __name__ == "__main__":
    unittest.main()
