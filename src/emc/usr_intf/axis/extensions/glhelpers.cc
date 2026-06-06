/*
 * glhelpers.cc — Python C extension for OpenGL geometry helpers.
 *
 * Extracted from emcmodule.cc to break the linuxcnc dependency.
 * Pure OpenGL + math — no NML, no stat, no HAL.
 *
 * Provides: draw_lines, draw_dwells, line9, vertex9,
 *           gui_respect_offsets, gui_rot_offsets
 */

#include <Python.h>
#include <cmath>
#include <cstring>
#include <epoxy/gl.h>

/* ─── Rotation offsets (module-level state) ─── */

#define AXIS_MASK_A 0x08
#define AXIS_MASK_B 0x10
#define AXIS_MASK_C 0x20

static struct rotation_offsets {
    double x;
    double y;
    double z;
    unsigned int axis_mask;
    unsigned int respect_offsets;
} roffsets;

/* ─── Geometry helpers ─── */

static void rotate_z(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double tx, ty;
    if (roffsets.respect_offsets) {
        tx = (pt[0]-roffsets.x) * c - (pt[1]-roffsets.y) * s;
        ty = (pt[0]-roffsets.x) * s + (pt[1]-roffsets.y) * c;
    } else {
        tx = pt[0] * c - pt[1] * s;
        ty = pt[0] * s + pt[1] * c;
    }
    pt[0] = tx; pt[1] = ty;
}

static void rotate_y(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double tx, tz;
    if (roffsets.respect_offsets) {
        tx = (pt[0]-roffsets.x) * c - (pt[2]-roffsets.z) * s;
        tz = (pt[0]-roffsets.x) * s + (pt[2]-roffsets.z) * c;
    } else {
        tx = pt[0] * c - pt[2] * s;
        tz = pt[0] * s + pt[2] * c;
    }
    pt[0] = tx; pt[2] = tz;
}

static void rotate_x(double pt[3], double a) {
    double theta = a * M_PI / 180;
    double c = cos(theta), s = sin(theta);
    double ty, tz;
    if (roffsets.respect_offsets) {
        ty = (pt[1]-roffsets.y) * c - (pt[2]-roffsets.z) * s;
        tz = (pt[1]-roffsets.y) * s + (pt[2]-roffsets.z) * c;
    } else {
        ty = pt[1] * c - pt[2] * s;
        tz = pt[1] * s + pt[2] * c;
    }
    pt[1] = ty; pt[2] = tz;
}

static void translate(double pt[3], double ox, double oy, double oz) {
    pt[0] += ox;
    pt[1] += oy;
    pt[2] += oz;
}

static void vertex9(const double pt[9], double p[3], const char *geometry) {
    double sign = 1;
    p[0] = 0; p[1] = 0; p[2] = 0;

    for(; *geometry; geometry++) {
        switch(*geometry) {
            case '-': sign = -1; break;
            case 'X': translate(p, pt[0] * sign, 0, 0); sign=1; break;
            case 'Y': translate(p, 0, pt[1] * sign, 0); sign=1; break;
            case 'Z': translate(p, 0, 0, pt[2] * sign); sign=1; break;
            case 'U': translate(p, pt[6] * sign, 0, 0); sign=1; break;
            case 'V': translate(p, 0, pt[7] * sign, 0); sign=1; break;
            case 'W': translate(p, 0, 0, pt[8] * sign); sign=1; break;
            case 'A': if (roffsets.axis_mask & AXIS_MASK_A)
                          rotate_x(p, pt[3] * sign);
                      sign=1; break;
            case 'B': if (roffsets.axis_mask & AXIS_MASK_B)
                          rotate_y(p, pt[4] * sign);
                      sign=1; break;
            case 'C': if (roffsets.axis_mask & AXIS_MASK_C)
                          rotate_z(p, pt[5] * sign);
                      sign=1; break;
        }
    }
}

static void glvertex9(const double pt[9], const char *geometry) {
    double p[3];
    vertex9(pt, p, geometry);
    glVertex3dv(p);
}

#define max(a,b) ((a) < (b) ? (b) : (a))
#define max3(a,b,c) (max((a),max((b),(c))))

static void line9_impl(const double p1[9], const double p2[9], const char *geometry) {
    if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
        double dc = max3(
            fabs(p2[3] - p1[3]),
            fabs(p2[4] - p1[4]),
            fabs(p2[5] - p1[5]));
        int st = (int)ceil(max(10, dc/10));
        for(int i=1; i<=st; i++) {
            double t = i * 1.0 / st;
            double v = 1.0 - t;
            double pt[9];
            for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
            glvertex9(pt, geometry);
        }
    } else {
        glvertex9(p2, geometry);
    }
}

static void line9b_impl(const double p1[9], const double p2[9], const char *geometry) {
    glvertex9(p1, geometry);
    if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
        double dc = max3(
            fabs(p2[3] - p1[3]),
            fabs(p2[4] - p1[4]),
            fabs(p2[5] - p1[5]));
        int st = (int)ceil(max(10, dc/10));
        for(int i=1; i<=st; i++) {
            double t = i * 1.0 / st;
            double v = 1.0 - t;
            double pt[9];
            for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
            glvertex9(pt, geometry);
            if(i != st)
                glvertex9(pt, geometry);
        }
    } else {
        glvertex9(p2, geometry);
    }
}

/* ─── Python wrappers ─── */

static PyObject *py_line9(PyObject *s, PyObject *o) {
    double pt1[9], pt2[9];
    const char *geometry;

    if(!PyArg_ParseTuple(o, "s(ddddddddd)(ddddddddd):line9",
            &geometry,
            &pt1[0], &pt1[1], &pt1[2],
            &pt1[3], &pt1[4], &pt1[5],
            &pt1[6], &pt1[7], &pt1[8],
            &pt2[0], &pt2[1], &pt2[2],
            &pt2[3], &pt2[4], &pt2[5],
            &pt2[6], &pt2[7], &pt2[8]))
        return NULL;

    line9b_impl(pt1, pt2, geometry);
    Py_RETURN_NONE;
}

static PyObject *py_vertex9(PyObject *s, PyObject *o) {
    double pt1[9], pt[3];
    char *geometry;
    if(!PyArg_ParseTuple(o, "s(ddddddddd):vertex9",
            &geometry,
            &pt1[0], &pt1[1], &pt1[2],
            &pt1[3], &pt1[4], &pt1[5],
            &pt1[6], &pt1[7], &pt1[8]))
        return NULL;

    vertex9(pt, pt1, geometry);
    return Py_BuildValue("(ddd)", &pt[0], &pt[1], &pt[2]);
}

static PyObject *py_gui_respect_offsets(PyObject *s, PyObject *o) {
    char *coords;
    if(!PyArg_ParseTuple(o, "si", &coords, &roffsets.respect_offsets))
        return NULL;
    if (roffsets.respect_offsets) {
        roffsets.axis_mask = 0;
        if (strchr(coords,'A')) roffsets.axis_mask |= AXIS_MASK_A;
        if (strchr(coords,'B')) roffsets.axis_mask |= AXIS_MASK_B;
        if (strchr(coords,'C')) roffsets.axis_mask |= AXIS_MASK_C;
    }
    Py_RETURN_NONE;
}

static PyObject *py_gui_rot_offsets(PyObject *s, PyObject *o) {
    if(!PyArg_ParseTuple(o, "ddd", &roffsets.x, &roffsets.y, &roffsets.z))
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *py_draw_lines(PyObject *s, PyObject *o) {
    PyListObject *li;
    int for_selection = 0;
    int first = 1;
    int nl = -1, n;
    double p1[9], p2[9], pl[9];
    char *geometry;

    if(!PyArg_ParseTuple(o, "sO!|i:draw_lines",
                &geometry, &PyList_Type, &li, &for_selection))
        return NULL;

    for(int i=0; i<PyList_GET_SIZE(li); i++) {
        PyObject *it = PyList_GET_ITEM(li, i);
        PyObject *dummy1, *dummy2, *dummy3;
        if(!PyArg_ParseTuple(it, "i(ddddddddd)(ddddddddd)|OOO", &n,
                    p1+0, p1+1, p1+2,
                    p1+3, p1+4, p1+5,
                    p1+6, p1+7, p1+8,
                    p2+0, p2+1, p2+2,
                    p2+3, p2+4, p2+5,
                    p2+6, p2+7, p2+8,
                    &dummy1, &dummy2, &dummy3)) {
            if(!first) glEnd();
            return NULL;
        }
        if(first || memcmp(p1, pl, sizeof(p1))
                || (for_selection && n != nl)) {
            if(!first) glEnd();
            if(for_selection && n != nl) {
                glLoadName(n);
                nl = n;
            }
            glBegin(GL_LINE_STRIP);
            glvertex9(p1, geometry);
            first = 0;
        }
        line9_impl(p1, p2, geometry);
        memcpy(pl, p2, sizeof(p1));
    }

    if(!first) glEnd();
    Py_RETURN_NONE;
}

static PyObject *py_draw_dwells(PyObject *s, PyObject *o) {
    PyListObject *li;
    int for_selection = 0, is_lathe = 0, n;
    double alpha;
    char *geometry;
    double delta = 0.015625;

    if(!PyArg_ParseTuple(o, "sO!dii:draw_dwells",
                &geometry, &PyList_Type, &li, &alpha, &for_selection, &is_lathe))
        return NULL;

    if (for_selection == 0)
        glBegin(GL_LINES);

    for(int i=0; i<PyList_GET_SIZE(li); i++) {
        PyObject *it = PyList_GET_ITEM(li, i);
        double red, green, blue, x, y, z;
        int axis;
        if(!PyArg_ParseTuple(it, "i(ddd)dddi", &n, &red, &green, &blue, &x, &y, &z, &axis))
            return NULL;
        if (for_selection != 1)
            glColor4d(red, green, blue, alpha);
        if (for_selection == 1) {
            glLoadName(n);
            glBegin(GL_LINES);
        }
        if (is_lathe == 1)
            axis = 1;

        if (axis == 0) {
            glVertex3f(x-delta,y-delta,z);
            glVertex3f(x+delta,y+delta,z);
            glVertex3f(x-delta,y+delta,z);
            glVertex3f(x+delta,y-delta,z);
            glVertex3f(x+delta,y+delta,z);
            glVertex3f(x-delta,y-delta,z);
            glVertex3f(x+delta,y-delta,z);
            glVertex3f(x-delta,y+delta,z);
        } else if (axis == 1) {
            glVertex3f(x-delta,y,z-delta);
            glVertex3f(x+delta,y,z+delta);
            glVertex3f(x-delta,y,z+delta);
            glVertex3f(x+delta,y,z-delta);
            glVertex3f(x+delta,y,z+delta);
            glVertex3f(x-delta,y,z-delta);
            glVertex3f(x+delta,y,z-delta);
            glVertex3f(x-delta,y,z+delta);
        } else {
            glVertex3f(x,y-delta,z-delta);
            glVertex3f(x,y+delta,z+delta);
            glVertex3f(x,y+delta,z-delta);
            glVertex3f(x,y-delta,z+delta);
            glVertex3f(x,y+delta,z+delta);
            glVertex3f(x,y-delta,z-delta);
            glVertex3f(x,y-delta,z+delta);
            glVertex3f(x,y+delta,z-delta);
        }
        if (for_selection == 1)
            glEnd();
    }

    if (for_selection == 0)
        glEnd();

    Py_RETURN_NONE;
}

/* ─── Module definition ─── */

static PyMethodDef glhelpers_methods[] = {
    {"draw_lines", py_draw_lines, METH_VARARGS,
        "Draw G-code lines with 9-axis geometry transformation"},
    {"draw_dwells", py_draw_dwells, METH_VARARGS,
        "Draw dwell markers"},
    {"line9", py_line9, METH_VARARGS,
        "Draw a single line with 9-axis geometry transformation"},
    {"vertex9", py_vertex9, METH_VARARGS,
        "Get the 3D location for a 9-axis point"},
    {"gui_respect_offsets", py_gui_respect_offsets, METH_VARARGS,
        "Set which axes have rotational offsets"},
    {"gui_rot_offsets", py_gui_rot_offsets, METH_VARARGS,
        "Set the rotation offset values"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef glhelpers_module = {
    PyModuleDef_HEAD_INIT,
    "_glhelpers",
    "OpenGL geometry helpers for LinuxCNC backplot rendering.\n"
    "Extracted from linuxcnc module — no NML/stat/HAL dependency.",
    -1,
    glhelpers_methods
};

PyMODINIT_FUNC PyInit__glhelpers(void) {
    return PyModule_Create(&glhelpers_module);
}
