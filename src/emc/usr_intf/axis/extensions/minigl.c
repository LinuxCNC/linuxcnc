//    This is a component of AXIS, a front-end for emc
//    Copyright 2005, 2006 Jeff Epler <jepler@unpythonic.net> and 
//    Chris Radek <chris@timeguy.com>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <Python.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>

static PyObject *glerror;

#define GLCALL0V(name) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    if(!PyArg_ParseTuple(o, "")) return NULL; \
    name(); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}
#define GLCALL1V(name, fmt, t1) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    t1 p1; \
    if(!PyArg_ParseTuple(o, fmt, &p1)) return NULL; \
    name(p1); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}
#define GLCALL2V(name, fmt, t1, t2) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    t1 p1; t2 p2; \
    if(!PyArg_ParseTuple(o, fmt, &p1, &p2)) return NULL; \
    name(p1, p2); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}
#define GLCALL3V(name, fmt, t1, t2, t3) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    t1 p1; t2 p2; t3 p3; \
    if(!PyArg_ParseTuple(o, fmt, &p1, &p2, &p3)) return NULL; \
    name(p1, p2, p3); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}

#define GLCALL4V(name, fmt, t1, t2, t3, t4) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    t1 p1; t2 p2; t3 p3; t4 p4; \
    if(!PyArg_ParseTuple(o, fmt, &p1, &p2, &p3, &p4)) return NULL; \
    name(p1, p2, p3, p4); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}

#define GLCALL6V(name, fmt, t1, t2, t3, t4, t5, t6) \
static PyObject *py##name(PyObject *s, PyObject *o) { \
    t1 p1; t2 p2; t3 p3; t4 p4; t5 p5; t6 p6; \
    if(!PyArg_ParseTuple(o, fmt, &p1, &p2, &p3, &p4, &p5, &p6)) return NULL; \
    name(p1, p2, p3, p4, p5, p6); \
    CHECK_ERROR; \
    Py_INCREF(Py_None); return Py_None; \
}

#define CHECK_ERROR (void)0

GLCALL1V(glBegin, "i", int)
GLCALL3V(glColor3f, "fff", float, float, float)
GLCALL4V(glColor4f, "ffff", float, float, float, float)
GLCALL4V(glBlendColor, "ffff", float, float, float, float)
GLCALL3V(glVertex3f, "fff", float, float, float);
GLCALL2V(glLineStipple, "ii", int, int)
GLCALL1V(glLineWidth, "f", float)

GLCALL1V(glCallList, "i", int)
GLCALL1V(glClear, "i", int)
GLCALL4V(glClearColor, "ffff", float, float, float, float)
GLCALL1V(glDepthFunc, "i", int)
GLCALL1V(glDepthMask, "i", int)
GLCALL1V(glDisable, "i", int)
GLCALL1V(glEnable, "i", int)
GLCALL0V(glEndList)
GLCALL1V(glFrontFace, "i", int)
GLCALL0V(glInitNames)
GLCALL3V(glLightf, "iif", int, int, float)
GLCALL0V(glLoadIdentity)
GLCALL1V(glLoadName, "i", int)
GLCALL2V(glNewList, "ii", int, int)
GLCALL3V(glNormal3f, "fff", float, float, float)
GLCALL2V(glPolygonOffset, "ff", float, float)
GLCALL0V(glPopMatrix)
GLCALL0V(glPushMatrix)
GLCALL1V(glPushName, "i", int)
GLCALL2V(glRasterPos2i, "ii", int, int)
GLCALL4V(glRectf, "ffff", float, float, float, float)
GLCALL4V(glRotatef, "ffff", float, float, float, float)
GLCALL3V(glScalef, "fff", float, float, float)
GLCALL1V(glDrawBuffer, "i", int)
GLCALL3V(glDrawArrays, "iii", int, int, int)
GLCALL1V(glMatrixMode, "i", int)
GLCALL6V(glOrtho, "ffffff", float, float, float, float, float, float);
GLCALL3V(glTranslatef, "fff", float, float, float);
GLCALL4V(glViewport, "iiii", int, int, int, int);
GLCALL4V(gluPerspective, "dddd", double, double, double, double);

static void make_glerror(int code) {
    PyObject *e = \
        PyObject_CallFunction(glerror, "is", code, gluErrorString(code));
    PyErr_SetObject(glerror, e);
}

#undef CHECK_ERROR
#define CHECK_ERROR do { \
    GLenum e = glGetError(); \
    if(e) { make_glerror(e); return NULL; } \
} while(0) 

GLCALL0V(glEnd)
GLCALL2V(glDeleteLists, "ii", int, int)
GLCALL2V(glBlendFunc, "ii", int, int)
GLCALL0V(glFlush)
GLCALL2V(glPixelStorei, "ii", int, int)

static PyObject *pyglBitmap(PyObject *s, PyObject *o) {
    int width, height, nbitmap;
    float xorg, yorg, xmove, ymove;
    char *bitmap;
    if(!PyArg_ParseTuple(o, "iiffffs#", &width, &height,
                &xorg, &yorg, &xmove, &ymove, &bitmap, &nbitmap)) {
        return NULL;
    }
    glBitmap(width, height, xorg, yorg, xmove, ymove, (GLubyte*)bitmap);
    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglGenLists(PyObject *s, PyObject *o) {
    int range;
    if(!PyArg_ParseTuple(o, "i", &range)) return NULL;
    return PyInt_FromLong(glGenLists(range));
}

static PyObject *pyglGetDoublev(PyObject *s, PyObject *o) {
    int what;
    if(!PyArg_ParseTuple(o, "i", &what)) return NULL;
    switch(what) {
        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX: {
            double d[16];
            PyObject *r = PyList_New(16);
            int i;
            glGetDoublev(what, d);
            for(i=0; i<16; i++) {
                PyList_SetItem(r, i, PyFloat_FromDouble(d[i]));
            }
            return r;
        }
        default:
            PyErr_Format(PyExc_ValueError, "glGetDoublev does not support getting %d", what);
            return NULL;
    }
}

static PyObject *pyglGetIntegerv(PyObject *s, PyObject *o) {
    int what;
    if(!PyArg_ParseTuple(o, "i", &what)) return NULL;
    switch(what) {
        case GL_LIST_INDEX: {
            int r;
            glGetIntegerv(what, &r);
            return PyInt_FromLong(r);
        }
        case GL_VIEWPORT: {
            int d[4];
            PyObject *r = PyList_New(4);
            int i;
            glGetIntegerv(what, d);
            for(i=0; i<4; i++) {
                PyList_SetItem(r, i, PyInt_FromLong(d[i]));
            }
            return r;
        }
        default:
            PyErr_Format(PyExc_ValueError, "glGetIntegerv does not support getting %d", what);
            return NULL;
    }
}

static PyObject *pyglInterleavedArrays(PyObject *s, PyObject *o) {
    static void *buf = NULL;
    PyObject *str;
    int format, stride, size;

    if(!PyArg_ParseTuple(o, "iiO", &format, &stride, &str)) {
        return NULL;
    }

    if(!PyString_Check(str)) {
        PyErr_Format( PyExc_TypeError, "Expected string" );
        return NULL;
    }

    // size = min(8192, PyString_GET_SIZE(str));
    size = PyString_GET_SIZE(str);
    if(buf == NULL) buf = malloc(size);
    else buf = realloc(buf, size);
    memcpy(buf, PyString_AS_STRING(str), size);
    glInterleavedArrays(format, stride, buf);

    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglLightModeli(PyObject *s, PyObject *o) {
    int pname, param;
    if(!PyArg_ParseTuple(o, "ii", &pname, &param))
        return NULL;
    glLightModeli(pname, param);

    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglLightModelfv(PyObject *s, PyObject *o) {
    int pname;
    float param[4];
    if(!PyArg_ParseTuple(o, "i(ffff)", &pname, param, param+1, param+2, param+3))
        return NULL;
    glLightModelfv(pname, param);

    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglLightfv(PyObject *s, PyObject *o) {
    int light, pname;
    float param[4];
    if(!PyArg_ParseTuple(o, "ii(ffff)", &light, &pname, param, param+1, param+2, param+3))
        return NULL;
    glLightfv(light, pname, param);

    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglMultMatrixd(PyObject *s, PyObject *o) {
    double matrix[16];
    if(!PyArg_ParseTuple(o, "(dddddddddddddddd)",
            matrix, matrix+1, matrix+2, matrix+3,
            matrix+4, matrix+5, matrix+6, matrix+7,
            matrix+8, matrix+9, matrix+10, matrix+11,
            matrix+12, matrix+13, matrix+14, matrix+15)) return NULL;

    glMultMatrixd(matrix);

    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pyglPolygonStipple(PyObject *s, PyObject *o) {
    char *buf;
    int sz;
    if(!PyArg_ParseTuple(o, "s#", &buf, &sz)) return NULL;
    if(sz != 128) {
        PyErr_SetString(PyExc_ValueError, "Buffer must be 128 bytes long");
        return NULL;
    }
    glPolygonStipple((GLubyte*)buf);
    CHECK_ERROR;
    Py_INCREF(Py_None);
    return Py_None;
}

typedef struct {
    PyObject_HEAD
    GLUquadric *q;
} Quadric;
static void Quadric_dealloc(Quadric *q);
// static PyObject *Quadric_repr(Quadric *q);

static PyTypeObject Quadric_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                      /* ob_size */
    "minigl.quadric",       /* ob_name */
    sizeof(Quadric), /* ob_basicsize */
    0,                      /* ob_itemsize */
    /* methods */
    (destructor)Quadric_dealloc,/*tp_dealloc*/
    0,                      /*tp_print*/
    0,                      /*tp_getattr*/
    0,                      /*tp_setattr*/
    0,                      /*tp_compare*/
    0,                      /*tp_repr*/
    0,                      /*tp_as_number*/
    0,                      /*tp_as_sequence*/
    0,                      /*tp_as_mapping*/
    0,                      /*tp_hash*/
    0,                      /*tp_call*/
    0,                      /*tp_str*/
    0,                      /*tp_getattro*/
    0,                      /*tp_setattro*/
    0,                      /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,     /*tp_flags*/
    0,                      /*tp_doc*/
    0,                      /*tp_traverse*/
    0,                      /*tp_clear*/
    0,                      /*tp_richcompare*/
    0,                      /*tp_weaklistoffset*/
    0,                      /*tp_iter*/
    0,                      /*tp_iternext*/
    0,                      /*tp_methods*/
    0,                      /*tp_members*/
    0,                      /*tp_getset*/
    0,                      /*tp_base*/
    0,                      /*tp_dict*/
    0,                      /*tp_descr_get*/
    0,                      /*tp_descr_set*/
    0,                      /*tp_dictoffset*/
    0,                      /*tp_init*/
    PyType_GenericAlloc,    /*tp_alloc*/
    PyType_GenericNew,      /*tp_new*/
    0,                      /*tp_free*/
    0,                      /*tp_is_gc*/
};

static void Quadric_dealloc(Quadric *q) {
    if(q->q) { gluDeleteQuadric(q->q); }
    PyMem_DEL(q);
}

static Quadric *pygluNewQuadric(PyObject *s, PyObject *o) {
    Quadric *q = PyObject_New(Quadric, &Quadric_Type);
    if(q) q->q = gluNewQuadric();
    return q;
}

static PyObject *pygluDeleteQuadric(PyObject *s, PyObject *o) {
    Quadric *q;
    if(!PyArg_ParseTuple(o, "O!", &Quadric_Type, &q)) return NULL;
    if(q->q) { gluDeleteQuadric(q->q); q->q = NULL; }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluCylinder(PyObject *s, PyObject *o) {
    Quadric *q;
    double base, top, height;
    int slices, stacks;

    if(!PyArg_ParseTuple(o, "O!dddii", &Quadric_Type, &q, &base, &top,
                &height, &slices, &stacks))
        return NULL;

    if(!q->q) {
        PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
        return NULL;
    }

    gluCylinder(q->q, base, top, height, slices, stacks);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluDisk(PyObject *s, PyObject *o) {
    Quadric *q;
    double inner, outer;
    int slices, loops;

    if(!PyArg_ParseTuple(o, "O!ddii", &Quadric_Type, &q, &inner, &outer,
                &slices, &loops))
        return NULL;

    if(!q->q) {
        PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
        return NULL;
    }

    gluDisk(q->q, inner, outer, slices, loops);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluQuadricOrientation(PyObject *s, PyObject *o) {
    Quadric *q;
    int orient;
    if(!PyArg_ParseTuple(o, "O!i", &Quadric_Type, &q, &orient)) return NULL;
    if(!q->q) {
        PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
        return NULL;
    }
    gluQuadricOrientation(q->q, orient);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluLookAt(PyObject *s, PyObject *o) {
    double eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz;
    if(!PyArg_ParseTuple(o, "ddddddddd", &eyex, &eyey, &eyez,
                &centerx, &centery, &centerz, &upx, &upy, &upz))
        return NULL;
    gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluPickMatrix(PyObject *s, PyObject *o) {
    double x, y, delx, dely;
    int viewport[4];
    if(!PyArg_ParseTuple(o, "dddd(iiii)", &x, &y, &delx, &dely,
                viewport, viewport+1, viewport+2, viewport+3))
        return NULL;
    gluPickMatrix(x, y, delx, dely, viewport);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pygluProject(PyObject *s, PyObject *o) {
    double x, y, z, wx, wy, wz, model[16], proj[16];
    int viewport[4];

    if(!PyArg_ParseTuple(o, "ddd", &x, &y, &z))
        return NULL;
                
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);

    gluProject(x, y, z, model, proj, viewport, &wx, &wy, &wz);

    CHECK_ERROR;

    return Py_BuildValue("ddd", wx, wy, wz);
}

static PyObject *pygluUnProject(PyObject *s, PyObject *o) {
    double x, y, z, wx, wy, wz, model[16], proj[16];
    int viewport[4];

    if(!PyArg_ParseTuple(o, "ddd", &x, &y, &z))
        return NULL;
                
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);

    gluUnProject(x, y, z, model, proj, viewport, &wx, &wy, &wz);

    CHECK_ERROR;

    return Py_BuildValue("ddd", wx, wy, wz);
}


#if 0
static PyObject *pygluProject(PyObject *s, PyObject *o) {
    double x, y, z, model[16], proj[16], wx, wy, wz;
    int viewport[4];

    if(!PyArg_ParseTuple(o, "ddd(dddddddddddddddd)(dddddddddddddddd)(iiii)",
                &x, &y, &z,
                model, model+1, model+2, model+3,
                model+4, model+5, model+6, model+7,
                model+8, model+9, model+10, model+11,
                model+12, model+13, model+14, model+15,
                proj, proj+1, proj+2, proj+3,
                proj+4, proj+5, proj+6, proj+7,
                proj+8, proj+9, proj+10, proj+11,
                proj+12, proj+13, proj+14, proj+15,
                viewport, viewport+1, viewport+2, viewport+3))
        return NULL;
    gluProject(x, y, z, model, proj, viewport, &wx, &wy, &wz);

    CHECK_ERROR;

    return Py_BuildValue("ddd", wx, wy, wz);
}


static PyObject *pygluUnProject(PyObject *s, PyObject *o) {
    double x, y, z, model[16], proj[16], ox, oy, oz;
    int viewport[4];

    if(!PyArg_ParseTuple(o, "ddd(dddddddddddddddd)(dddddddddddddddd)(iiii)",
                &x, &y, &z,
                model, model+1, model+2, model+3,
                model+4, model+5, model+6, model+7,
                model+8, model+9, model+10, model+11,
                model+12, model+13, model+14, model+15,
                proj, proj+1, proj+2, proj+3,
                proj+4, proj+5, proj+6, proj+7,
                proj+8, proj+9, proj+10, proj+11,
                proj+12, proj+13, proj+14, proj+15,
                viewport, viewport+1, viewport+2, viewport+3))
        return NULL;
    gluUnProject(x, y, z, model, proj, viewport, &ox, &oy, &oz);

    CHECK_ERROR;

    return Py_BuildValue("ddd", ox, oy, oz);
}
#endif
static GLuint *select_buffer = NULL;
static PyObject *pyglSelectBuffer( PyObject *s, PyObject *o) {
    int sz;
    if(!PyArg_ParseTuple(o, "i", &sz))
        return NULL;
    if(select_buffer) select_buffer = realloc( select_buffer, sizeof(int) * sz);
    else select_buffer = malloc(sizeof(int) * sz);

    glSelectBuffer(sz, select_buffer);

    CHECK_ERROR;

    Py_INCREF(Py_None);
    return Py_None;

}

static PyObject *pyglRenderMode( PyObject *s, PyObject *o) {
    int mode, lastmode, count;
    if(!PyArg_ParseTuple(o, "i", &mode))
        return NULL;

    glGetIntegerv(GL_RENDER_MODE, &lastmode);
    count = glRenderMode(mode);

    CHECK_ERROR;

    if(lastmode == GL_SELECT) {
        PyObject *r = PyList_New(0);
        int i = 0;
        while(i < count) {
            PyObject *record = PyTuple_New(3);
            int namelen = select_buffer[i++];
            PyObject *name = PyList_New(namelen);
            int j;
            PyTuple_SetItem(record, 0,
                    PyFloat_FromDouble(select_buffer[i++] / 214748364.));
            PyTuple_SetItem(record, 1,
                    PyFloat_FromDouble(select_buffer[i++] / 214748364.));
            for(j=0; namelen; namelen--, j++, i++)
                PyList_SetItem(name, j, PyInt_FromLong(select_buffer[i]));
            PyTuple_SetItem(record, 2, name);
            PyList_Append(r, record);
            Py_DECREF(record);
        }
        return r;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *pydraw_lines(PyObject *s, PyObject *o) {
    PyListObject *li;
    int for_selection = 0;
    int i;
    int first = 1;
    int nl = -1, n;
    double lx=0, ly=0, lz=0;
    double p1[3], p2[3];

    if(!PyArg_ParseTuple(o, "O!|i", &PyList_Type, &li, &for_selection))
        return NULL;

    for(i=0; i<PyList_GET_SIZE(li); i++) {
        PyObject *it = PyList_GET_ITEM(li, i);
        if(!PyArg_ParseTuple(it, "i(ddd)(ddd)", &n,
                    p1, p1+1, p1+2, p2, p2+1, p2+2)) {
            if(!first) glEnd();
            return NULL;
        }
        
        if(first || p1[0] != lx || p1[1] != ly || p1[2] != lz 
                || (for_selection && n != nl)) {
            if(!first) glEnd();
            if(for_selection && n != nl) {
                glLoadName(n);
                nl = n;
            }
            glBegin(GL_LINE_STRIP);
            glVertex3dv(p1);
            first = 0;
        }
        glVertex3dv(p2);
        lx = p2[0]; ly = p2[1], lz = p2[2];
    }

    if(!first) glEnd();

    Py_INCREF(Py_None);
    return Py_None;
}

/*
glSelectBuffer
*/

static PyMethodDef methods[] = {
#define METH(name, doc) { #name, (PyCFunction) py##name, METH_VARARGS, doc }
METH(glBegin, ""),
METH(glColor3f, ""),
METH(glColor4f, ""),
METH(glBlendColor, ""),
METH(glDeleteLists, ""),
METH(glBlendFunc, ""),
METH(glCallList, ""),
METH(glClear, ""),
METH(glClearColor, ""),
METH(glDepthFunc, ""),
METH(glDepthMask, ""),
METH(glDisable, ""),
METH(glEnable, ""),
METH(glEnd, ""),
METH(glEndList, ""),
METH(glFlush, ""),
METH(glFrontFace, ""),
METH(glInitNames, ""),
METH(glLightf, ""),
METH(glLineStipple, ""),
METH(glLineWidth, ""),
METH(glLoadIdentity, ""),
METH(glLoadName, ""),
METH(glMatrixMode, ""),
METH(glNewList, ""),
METH(glNormal3f, ""),
METH(glPolygonOffset, ""),
METH(glPolygonStipple, ""),
METH(glPopMatrix, ""),
METH(glPushMatrix, ""),
METH(glPushName, ""),
METH(glRenderMode, ""),
METH(glRasterPos2i, ""),
METH(glRectf, ""),
METH(glRotatef, ""),
METH(glScalef, ""),
METH(glFlush, ""),
METH(glDrawBuffer, ""),
METH(glDrawArrays, ""),
METH(glMatrixMode, ""),
METH(glOrtho, ""),
METH(glTranslatef, ""),
METH(glVertex3f, ""),
METH(glViewport, ""),
METH(gluPerspective, ""),

METH(glGenLists, ""),
METH(glGetDoublev, ""),
METH(glGetIntegerv, ""),
METH(glInterleavedArrays, ""),
METH(glLightfv, ""),
METH(glLightModelfv, ""),
METH(glLightModeli, ""),
METH(glMultMatrixd, ""),
METH(glPixelStorei, ""),

METH(glSelectBuffer, ""),
// METH(glVertex3fv, ""),
METH(gluCylinder, ""),
METH(gluDeleteQuadric, ""),
METH(gluDisk, ""),
METH(gluLookAt, ""),
METH(gluNewQuadric, ""),
METH(gluPickMatrix, ""),
METH(gluProject, ""),
METH(gluQuadricOrientation, ""),
METH(gluUnProject, ""),
METH(glBitmap, ""),

METH(draw_lines, "Draw a bunch of lines in the 'rs274.glcanon' format"),

#undef METH
{NULL, NULL, 0, 0},
};

#define CONST(x) PyObject_SetAttrString(m, #x, PyInt_FromLong(x))
void initminigl(void) {
    PyObject *m = \
    Py_InitModule3("minigl", methods, "Mini version of pyopengl for axis");
    glerror = PyErr_NewException("minigl.error", PyExc_RuntimeError, NULL);
    PyObject_SetAttrString(m, "error", glerror);
    CONST(GL_ALWAYS);
    CONST(GL_LEQUAL);
    CONST(GL_BACK);
    CONST(GL_BLEND);
    CONST(GL_COLOR_BUFFER_BIT);
    CONST(GL_COMPILE);
    CONST(GL_CULL_FACE);
    CONST(GL_DEPTH_BUFFER_BIT);
    CONST(GL_DEPTH_TEST);
    CONST(GL_FALSE);
    CONST(GL_FRONT);
    CONST(GL_LESS);
    CONST(GL_LIGHTING);
    CONST(GL_LIGHTING);
    CONST(GL_LIGHT_MODEL_AMBIENT);
    CONST(GL_LIGHT_MODEL_LOCAL_VIEWER);
    CONST(GL_LINES);
    CONST(GL_LINE_LOOP);
    CONST(GL_LINE_STIPPLE);
    CONST(GL_LINE_STRIP);
    CONST(GL_MODELVIEW);
    CONST(GL_MODELVIEW_MATRIX);
    CONST(GL_ONE_MINUS_SRC_ALPHA);
    CONST(GL_CONSTANT_ALPHA);
    CONST(GL_ONE);
    CONST(GL_PROJECTION);
    CONST(GL_PROJECTION_MATRIX);
    CONST(GL_QUADS);
    CONST(GL_RENDER);
    CONST(GL_SELECT);
    CONST(GL_SRC_ALPHA);
    CONST(GL_STACK_OVERFLOW);
    CONST(GL_TRUE);
    CONST(GL_UNPACK_ALIGNMENT);
    CONST(GL_V3F);
    CONST(GL_C3F_V3F);
    CONST(GL_C4UB_V3F);
    CONST(GL_VIEWPORT);
    CONST(GL_LIGHT0);
    CONST(GL_POSITION);
    CONST(GL_AMBIENT);
    CONST(GL_DIFFUSE);
    CONST(GL_CCW);
    CONST(GL_DITHER);
    CONST(GL_AUTO_NORMAL);
    CONST(GL_NORMALIZE);
    CONST(GL_POLYGON_OFFSET_FILL);
    CONST(GL_POLYGON_STIPPLE);
    CONST(GL_POLYGON);
    CONST(GL_GREATER);
    CONST(GL_LIST_INDEX);
    CONST(GL_TRIANGLES);
    CONST(GL_TRIANGLE_FAN);
    CONST(GLU_INSIDE);
    CONST(GLU_OUTSIDE);
}
