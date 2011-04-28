// Interpreter access for Python oword subs
// this module assumes the parent process has librs274ngc.so loaded
// and hence cannot be used standalone as a normal Python import
//
// Michael Haberler April 2011

#include <Python.h>
#include <structmember.h>

#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "canon.hh"
#include "config.h"

// static char savedError[LINELEN+1];
// static PyObject *rs274_strerror(PyObject *s, PyObject *o) {
//     int err;
//     if(!PyArg_ParseTuple(o, "i", &err)) return NULL;
//     interp_new.error_text(err, savedError, LINELEN);
//     return PyString_FromString(savedError);
// }


static PyObject *
param(PyObject *self, PyObject *args) {
    int pnumber = -1;
    char *pname = NULL;
    PyObject *py_this;

    if (!PyArg_ParseTuple(args, "Oi", &py_this, &pnumber)) {
	if (!PyArg_ParseTuple(args, "Os", &py_this,&pname)) {
	    PyErr_SetString(PyExc_Exception,"expecting object and integer");
	    PyErr_Print();
	    return NULL;
	}
    }
}


static PyObject *
fib(PyObject *self, PyObject *args) {

    int a = 0, b = 1, c, n;
    PyObject *list = PyList_New(0);
    PyObject *number, *py_this;
    setup_pointer settings;

    if (!PyArg_ParseTuple(args, "Oi", &py_this,&n)) {
	PyErr_SetString(PyExc_Exception,"expecting object and integer");
        PyErr_Print();
        return NULL;
    }

    if (!PyCObject_Check(py_this)) {
	PyErr_SetString(PyExc_Exception,"first argument to fib() not a PyCObject");
	return NULL;
    }

    // Convert the PyCObject to a void pointer:
    void * temp = PyCObject_AsVoidPtr(py_this);
    // Cast the void pointer to an Interp instance:
    Interp *interp = static_cast<Interp *>(temp);

    // now we can call back into the Interpreter:
    interp->signal_error(4711);

    settings = interp->get_setup();
    // or access values:
    fprintf(stderr,"--- _setup.settings: %f %f %f\n",
	    settings->active_settings[0],
	    settings->active_settings[1],
	    settings->active_settings[2]);

    while (b < n) {
        number = PyInt_FromLong(b); // Need to get the pointer
        PyList_Append(list, number);
        Py_DECREF(number); // so I can DECREF it if necessary
        // see fix at above link by ivan
        // PyList_Append(list, PyInt_FromLong(b));
        c = a + b;
        a = b;
        b = c;
    }

    return list;
}

// call as doppel(int, string[,volt=24.0,ampere=1.2])
static PyObject *
doppel(PyObject *self, PyObject *args, PyObject *keywords) {

    float volt = 12.0;
    float ampere = 1.5;
    char *s;
    int i;

    static char *kwlist[] = { "volt","ampere",NULL};
    PyObject *val;

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "is|ff", kwlist,
				     &i, &s, &volt, &ampere))
	return NULL;

    fprintf(stderr,"i=%d s=\"%s\" volt=%.4f ampere=%.4f\n",i,s,volt,ampere);

    val  = PyFloat_FromDouble(volt*ampere);

    return val;
}

static PyMethodDef methods[] = {

    { "doppel", (PyCFunction)doppel, METH_VARARGS|METH_KEYWORDS, "double a float" },
    { "fib", fib, METH_VARARGS, "Returns a fibonacci sequence as a list" },
    { "param", param, METH_VARARGS, "return the value of a numeric or named parameter" },
    {0,0,0,0}
};


PyMODINIT_FUNC
initfib(void) {
    fprintf(stderr,"initfib called\n");

       (void) Py_InitModule3("fib", methods, "demo");
        PyErr_Print();
	// settings = get_setup();
}

// /* SWF.c

//    Python wrapper for librfxswf- module core.

//    Part of the swftools package.

//    Copyright (c) 2003 Matthias Kramm <kramm@quiss.org>

//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// #include <Python.h>
// #undef HAVE_STAT
// #include "../rfxswf.h"
// #include "../log.h"
// #include "./pyutils.h"
// #include "./tags.h"
// #include "./taglist.h"
// #include "./primitives.h"
// #include "./action.h"

// /*
// TODO:
//     1) taglist is rfxswflib's linked list. It should maybe implemented as Python
//        list, which would, however, mean that we would have to convert the list
//        back and forth for the following functions:
// 	load, save, writeCGI, unfoldAll, foldAll, optimizeOrder
//     2) taglist should have an ID handler. Every time a tag is inserted, it's ID
//        is stored in a lookup list.
//     3)
// */

// //-------------------------- Types -------------------------------------------

// staticforward PyTypeObject SWFClass;

// /* Tags, Objects */

// typedef struct {
//     PyObject_HEAD
//     SWF swf; //swf.firstTag is not used
//     PyObject*taglist;
//     char*filename;
// } SWFObject;


// //----------------------------------------------------------------------------
// static PyObject* f_create(PyObject* self, PyObject* args, PyObject* kwargs)
// {
//     static char *kwlist[] = {"version", "fps", "bbox", "name", NULL};
//     SWFObject* swf;
//     int version = 6;
//     double framerate = 25;
//     PyObject * obbox = 0;
//     SRECT bbox = {0,0,0,0};
//     char* filename = 0;

//     swf = PyObject_New(SWFObject, &SWFClass);
//     mylog("+%08x(%d) create\n", (int)swf, swf->ob_refcnt);

//     if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|idOs",
// 		kwlist, &version, &framerate,
// 		&obbox, &filename))
// 	return NULL;

//     if(obbox) {
// 	if (!PY_CHECK_TYPE(obbox, &BBoxClass)) {
// 	    obbox = f_BBox(0, obbox, 0);
// 	    if(!obbox)
// 		return NULL;
// 	}
// 	bbox = bbox_getSRECT(obbox);
//     }

//     memset(&swf->swf, 0, sizeof(SWF));
//     if(filename)
// 	swf->filename = strdup(filename);
//     else
// 	swf->filename = 0;

//     swf->swf.fileVersion = version;
//     swf->swf.frameRate = (int)(framerate*0x100);
//     swf->swf.movieSize = bbox;
//     swf->taglist = taglist_new();

//     if(swf->swf.fileVersion>=6)
// 	swf->swf.compressed = 1;

//     mylog(" %08x(%d) create: done\n", (int)swf, swf->ob_refcnt);
//     return (PyObject*)swf;
// }
// //----------------------------------------------------------------------------
// static PyObject* f_load(PyObject* self, PyObject* args, PyObject* kwargs)
// {
//     static char *kwlist1[] = {"filename", NULL};
//     static char *kwlist2[] = {"data", NULL};
//     char* filename = 0;
//     char* data = 0;
//     int len = 0;
//     SWFObject* swf;
//     int fi;

//     if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist1, &filename)) {
// 	PyErr_Clear();
// 	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#", kwlist2, &data, &len)) {
// 	    PyErr_Clear();
// 	    PyArg_ParseTupleAndKeywords(args, kwargs, "s:load", kwlist1, &filename);
// 	    return 0;
// 	}
//     }

//     swf = PyObject_New(SWFObject, &SWFClass);
//     mylog("+%08x(%d) f_load\n", (int)swf, swf->ob_refcnt);

//     memset(&swf->swf, 0, sizeof(SWF));

//     if(filename) {
// 	if(!filename) {
// 	    PyErr_SetString(PyExc_Exception, setECouldn't open file %s", filename));
// 	    return 0;
// 	}
// 	swf->filename = strdup(filename);
// 	fi = open(filename,O_RDONLY|O_BINARY);
// 	if (fi<0) {
// 	    return PY_ERROR("Couldn't open file %s", filename);
// 	}
// 	if(swf_ReadSWF(fi,&swf->swf)<0) {
// 	    close(fi);
// 	    return PY_ERROR("%s is not a valid SWF file or contains errors",filename);
// 	}
// 	close(fi);
//     } else {
// 	reader_t r;
// 	reader_init_memreader(&r, data, len);
// 	swf->filename = 0;
// 	if(swf_ReadSWF2(&r, &swf->swf)<0) {
// 	    return PY_ERROR("<data> is not a valid SWF file or contains errors");
// 	}
// 	r.dealloc(&r);
//     }
//     swf_FoldAll(&swf->swf);

//     swf->taglist = taglist_new2(swf->swf.firstTag);
//     if(swf->taglist == NULL) {
// 	return NULL;
//     }

//     swf_FreeTags(&swf->swf);
//     swf->swf.firstTag = 0;

//     return (PyObject*)swf;
// }
// //----------------------------------------------------------------------------
// static PyObject * swf_save(PyObject* self, PyObject* args, PyObject* kwargs)
// {
//     static char *kwlist[] = {"name", "compress", NULL};
//     SWFObject*swfo;
//     SWF*swf;
//     int fi;
//     char*filename = 0;
//     int compress = 0;

//     if(!self)
// 	return NULL;

//     swfo = (SWFObject*)self;
//     swf = &swfo->swf;

//     filename = swfo->filename;

//     if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|si", kwlist, &filename, &compress))
// 	return NULL;

//     mylog(" %08x(%d) f_save filename=%s compress=%d\n", (int)self, self->ob_refcnt, filename, compress);

//     // keyword arg compress (=1) forces compression
//     if(compress)
// 	swf->compressed = 1;

//     swf->firstTag = taglist_getTAGs(swfo->taglist);

//     /*if(!swf->firstTag)
// 	return NULL;*/

//     // fix the file, in case it is empty or not terminated properly
//     {
// 	TAG*tag = swf->firstTag;
// 	if(!tag)
// 	    tag = swf->firstTag = swf_InsertTag(0,ST_END);
// 	while(tag && tag->next) {
// 	    tag = tag->next;
// 	}
// 	if(tag->id != ST_END) {
// 	    tag = swf_InsertTag(tag,ST_END);
// 	}
//     }

//     fi = open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644);
//     if(fi<0) {
// 	PyErr_SetString(PyExc_Exception, setError("couldn't create output file %s", filename));
// 	return 0;
//     }
//     if(swf_WriteSWF(fi, swf)<0) {
//         close(fi);
//         PyErr_SetString(PyExc_Exception, setError("WriteSWC() failed."));
//         return 0;
//     }
//     close(fi);

//     swf_FreeTags(swf);
//     /*{ TAG * t = swf->firstTag;
//       while (t)
//       {
// 	mylog("tag: %08x\n",t);
// 	mylog("  id: %d (%s)\n", t->id, swf_TagGetName(t));
// 	mylog("  data: %08x (%d bytes)\n", t->data, t->len);
// 	mylog("  next: %08x\n", t->next);
// 	TAG * tnew = t->next;
// 	mylog("->free data\n");
// 	if (t->data) free(t->data);
// 	mylog("->free tag\n");
// 	free(t);
// 	t = tnew;
//       }
//     }*/
//     swf->firstTag = 0;

//     mylog(" %08x(%d) f_save filename=%s done\n", (int)self, self->ob_refcnt, filename);

//     return PY_NONE;
// }
// //----------------------------------------------------------------------------
// static PyObject * swf_writeCGI(PyObject* self, PyObject* args)
// {
//     SWFObject*swf = (SWFObject*)self;
//     if(!self || !PyArg_ParseTuple(args,""))
// 	return NULL;
//     swf->swf.firstTag = taglist_getTAGs(swf->taglist);
//     if(!swf->swf.firstTag)
// 	return NULL;
//     swf_WriteCGI(&swf->swf);
//     swf_FreeTags(&swf->swf);
//     swf->swf.firstTag = 0;
//     return PY_NONE;
// }
// //----------------------------------------------------------------------------

// //TODO: void swf_Relocate(SWF*swf, char*bitmap); // bitmap is 65536 bytes, bitmap[a]==0 means id a is free

// static PyMethodDef swf_functions[] =
// {{"save", (PyCFunction)swf_save, METH_KEYWORDS, "Save SWF to disk"},
//  {"writeCGI", (PyCFunction)swf_writeCGI, METH_VARARGS, "print SWF as CGI to stdout"},
//  {NULL, NULL, 0, NULL}
// };

// //----------------------------------------------------------------------------
// static void swf_dealloc(PyObject* self)
// {
//     mylog("-%08x(%d) swf_dealloc\n", (int)self, self->ob_refcnt);
//     SWFObject*swfo;
//     SWF*swf;
//     swfo = (SWFObject*)self;
//     swf = &swfo->swf;
//     if(swfo->filename) {
// 	free(swfo->filename);
// 	swfo->filename = 0;
//     }
//     Py_DECREF(swfo->taglist);
//     swfo->taglist = 0;
//     PyObject_Del(self);
// }
// //----------------------------------------------------------------------------
// static int swf_print(PyObject * self, FILE *fi, int flags) //flags&Py_PRINT_RAW
// {
//     mylog(" %08x(%d) print \n", (int)self, self->ob_refcnt);
//     SWFObject*swf = (SWFObject*)self;
//     swf_DumpHeader(fi, &swf->swf);
//     //void swf_DumpSWF(FILE * f,SWF*swf);
//     return 0;
// }
// //----------------------------------------------------------------------------
// static PyObject* swf_getattr(PyObject * self, char* a)
// {
//     SWFObject*swf = (SWFObject*)self;
//     PyObject* ret = 0;

//     if(!strcmp(a, "fps")) {
// 	double fps = swf->swf.frameRate/256.0;
// 	mylog(" %08x(%d) swf_getattr %s = %f\n", (int)self, self->ob_refcnt, a, fps);
// 	return Py_BuildValue("d", fps);
//     } else if(!strcmp(a, "version")) {
// 	int version = swf->swf.fileVersion;;
// 	mylog(" %08x(%d) swf_getattr %s = %d\n", (int)self, self->ob_refcnt, a, version);
// 	return Py_BuildValue("i", version);
//     } else if(!strcmp(a, "name")) {
// 	char*filename = swf->filename;
// 	mylog(" %08x(%d) swf_getattr %s = %s\n", (int)self, self->ob_refcnt, a, filename);
// 	return Py_BuildValue("s", filename);
//     } else if(!strcmp(a, "bbox")) {
// 	return f_BBox2(swf->swf.movieSize);
//     } else if(!strcmp(a, "tags")) {
// 	PyObject*ret =  (PyObject*)(swf->taglist);
// 	Py_INCREF(ret);
// 	mylog(" %08x(%d) swf_getattr %s = %08x(%d)\n", (int)self, self->ob_refcnt, a, ret, ret->ob_refcnt);
// 	return ret;
//     } else if(!strcmp(a, "filesize")) {
// 	int s = swf->swf.fileSize;
// 	return Py_BuildValue("i", s);
//     } else if(!strcmp(a, "width")) {
// 	int w = (swf->swf.movieSize.xmax - swf->swf.movieSize.xmin) / 20;
// 	return Py_BuildValue("i", w);
//     } else if(!strcmp(a, "height")) {
// 	int h =  (swf->swf.movieSize.ymax - swf->swf.movieSize.ymin) / 20;
// 	return Py_BuildValue("i", h);
//     } else if(!strcmp(a, "framecount")) {
// 	int f = swf->swf.frameCount;
// 	return Py_BuildValue("i", f);
//     }

//     ret = Py_FindMethod(swf_functions, self, a);
//     mylog(" %08x(%d) swf_getattr %s: %08x\n", (int)self, self->ob_refcnt, a, ret);
//     return ret;
// }
// //----------------------------------------------------------------------------
// static int swf_setattr(PyObject * self, char* a, PyObject * o)
// {
//     SWFObject*swf = (SWFObject*)self;
//     if(!strcmp(a, "fps")) {
// 	double fps;
// 	if (!PyArg_Parse(o, "d", &fps))
// 	    goto err;
// 	swf->swf.frameRate = (int)(fps*0x100);
// 	mylog(" %08x(%d) swf_setattr %s = %f\n", (int)self, self->ob_refcnt, a, fps);
// 	return 0;
//     } else if(!strcmp(a, "version")) {
// 	int version;
// 	if (!PyArg_Parse(o, "i", &version))
// 	    goto err;
// 	swf->swf.fileVersion = version;
// 	mylog(" %08x(%d) swf_setattr %s = %d\n", (int)self, self->ob_refcnt, a, version);
// 	return 0;
//     } else if(!strcmp(a, "name")) {
// 	char*filename;
// 	if (!PyArg_Parse(o, "s", &filename))
// 	    goto err;
// 	if(swf->filename) {
// 	    free(swf->filename);swf->filename=0;
// 	}
// 	swf->filename = strdup(filename);
// 	mylog(" %08x(%d) swf_setattr %s = %s\n", (int)self, self->ob_refcnt, a, filename);
// 	return 0;
//     } else if(!strcmp(a, "bbox")) {
// 	PyObject *obbox = o;
// 	if (!PY_CHECK_TYPE(obbox, &BBoxClass)) {
// 	    obbox = f_BBox(0, o, 0);
// 	    if(!obbox)
// 		return 1;
// 	}
// 	SRECT bbox = bbox_getSRECT(obbox);

// 	swf->swf.movieSize = bbox;
// 	mylog(" %08x(%d) swf_setattr %s = (%d,%d,%d,%d)\n", (int)self, self->ob_refcnt, a, bbox.xmin,bbox.ymin,bbox.xmax,bbox.ymax);
// 	return 0;
//     } else if(!strcmp(a, "tags")) {
// 	PyObject* taglist;
// 	taglist = o;
// 	PY_ASSERT_TYPE(taglist,&TagListClass);
// 	Py_DECREF(swf->taglist);
// 	swf->taglist = taglist;
// 	Py_INCREF(swf->taglist);
// 	mylog(" %08x(%d) swf_setattr %s = %08x\n", (int)self, self->ob_refcnt, a, swf->taglist);
// 	return 0;
//     }
// err:
//     mylog(" %08x(%d) swf_setattr %s = ? (%08x)\n", (int)self, self->ob_refcnt, a, o);
//     return 1;
// }

// //----------------------------------------------------------------------------
// static PyTypeObject SWFClass =
// {
//     PyObject_HEAD_INIT(NULL)
//     0,
//     tp_name: "SWF",
//     tp_basicsize: sizeof(SWFObject),
//     tp_itemsize: 0,
//     tp_dealloc: swf_dealloc,
//     tp_print: swf_print,
//     tp_getattr: swf_getattr,
//     tp_setattr: swf_setattr,
// };
// //----------------------------------------------------------------------------

// static PyMethodDef SWFMethods[] =
// {
//     /* SWF creation*/
//     {"load", (PyCFunction)f_load, METH_KEYWORDS, "Load a SWF from disc."},
//     {"create", (PyCFunction)f_create, METH_KEYWORDS, "Create a new SWF from scratch."},
//     {0,0,0,0}
//     // save is a member function
// };
// PyMethodDef* swf_getMethods()
// {
//     SWFClass.ob_type = &PyType_Type;
//     return SWFMethods;
// }

// // =============================================================================

// #include "primitives.h"
// #include "action.h"
// #include "tag.h"
// #include "taglist.h"

// static PyObject* module_verbose(PyObject* self, PyObject* args, PyObject* kwargs)
// {
//     int _verbose = 0;
//     static char *kwlist[] = {"verbosity", NULL};
//     if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i", kwlist, &_verbose))
// 	return NULL;
//     setVerbosity(_verbose);

//     return Py_BuildValue("s", 0);
// }

// static PyMethodDef LoggingMethods[] =
// {
//     /* Module functions */
//     {"verbose", (PyCFunction)module_verbose, METH_KEYWORDS, "Set the module verbosity"},
//     {0,0,0,0}
// };

// void initSWF(void)
// {
//     PyObject*module;
//     PyMethodDef* primitive_methods = primitive_getMethods();
//     PyMethodDef* tag_methods = tags_getMethods();
//     PyMethodDef* action_methods = action_getMethods();
//     PyMethodDef* swf_methods = swf_getMethods();

//     PyMethodDef* all_methods = 0;
//     all_methods = addMethods(all_methods, primitive_methods);
//     all_methods = addMethods(all_methods, tag_methods);
//     all_methods = addMethods(all_methods, action_methods);
//     all_methods = addMethods(all_methods, swf_methods);

//     all_methods = addMethods(all_methods, LoggingMethods);

//     module = Py_InitModule("SWF", all_methods);

//     /* Python doesn't copy the PyMethodDef struct, so we need
//        to keep it around */
//     // free(all_methods)
// }

//    This is a component of AXIS, a front-end for emc
//    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
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

// #include <Python.h>
// #include <structmember.h>

// #include "rs274ngc.hh"
// #include "rs274ngc_interp.hh"
// #include "interp_return.hh"
// #include "canon.hh"
// #include "config.h"		// LINELEN

// char _parameter_file_name[LINELEN];

// static PyObject *int_array(int *arr, int sz) {
//     PyObject *res = PyTuple_New(sz);
//     for(int i = 0; i < sz; i++) {
//         PyTuple_SET_ITEM(res, i, PyInt_FromLong(arr[i]));
//     }
//     return res;
// }

// typedef struct {
//     PyObject_HEAD
//     double settings[ACTIVE_SETTINGS];
//     int gcodes[ACTIVE_G_CODES];
//     int mcodes[ACTIVE_M_CODES];
// } LineCode;

// static PyObject *LineCode_gcodes(LineCode *l) {
//     return int_array(l->gcodes, ACTIVE_G_CODES);
// }
// static PyObject *LineCode_mcodes(LineCode *l) {
//     return int_array(l->mcodes, ACTIVE_M_CODES);
// }

// static PyGetSetDef LineCodeGetSet[] = {
//     {(char*)"gcodes", (getter)LineCode_gcodes},
//     {(char*)"mcodes", (getter)LineCode_mcodes},
//     {NULL, NULL},
// };

// static PyMemberDef LineCodeMembers[] = {
//     {(char*)"sequence_number", T_INT, offsetof(LineCode, gcodes[0]), READONLY},

//     {(char*)"feed_rate", T_DOUBLE, offsetof(LineCode, settings[1]), READONLY},
//     {(char*)"speed", T_DOUBLE, offsetof(LineCode, settings[2]), READONLY},
//     {(char*)"motion_mode", T_INT, offsetof(LineCode, gcodes[1]), READONLY},
//     {(char*)"block", T_INT, offsetof(LineCode, gcodes[2]), READONLY},
//     {(char*)"plane", T_INT, offsetof(LineCode, gcodes[3]), READONLY},
//     {(char*)"cutter_side", T_INT, offsetof(LineCode, gcodes[4]), READONLY},
//     {(char*)"units", T_INT, offsetof(LineCode, gcodes[5]), READONLY},
//     {(char*)"distance_mode", T_INT, offsetof(LineCode, gcodes[6]), READONLY},
//     {(char*)"feed_mode", T_INT, offsetof(LineCode, gcodes[7]), READONLY},
//     {(char*)"origin", T_INT, offsetof(LineCode, gcodes[8]), READONLY},
//     {(char*)"tool_length_offset", T_INT, offsetof(LineCode, gcodes[9]), READONLY},
//     {(char*)"retract_mode", T_INT, offsetof(LineCode, gcodes[10]), READONLY},
//     {(char*)"path_mode", T_INT, offsetof(LineCode, gcodes[11]), READONLY},

//     {(char*)"stopping", T_INT, offsetof(LineCode, mcodes[1]), READONLY},
//     {(char*)"spindle", T_INT, offsetof(LineCode, mcodes[2]), READONLY},
//     {(char*)"toolchange", T_INT, offsetof(LineCode, mcodes[3]), READONLY},
//     {(char*)"mist", T_INT, offsetof(LineCode, mcodes[4]), READONLY},
//     {(char*)"flood", T_INT, offsetof(LineCode, mcodes[5]), READONLY},
//     {(char*)"overrides", T_INT, offsetof(LineCode, mcodes[6]), READONLY},
//     {NULL}
// };

// static PyTypeObject LineCodeType = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "gcode.linecode",       /*tp_name*/
//     sizeof(LineCode),       /*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     0,                      /*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     0,                      /*tp_methods*/
//     LineCodeMembers,     /*tp_members*/
//     LineCodeGetSet,      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     0,                      /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// static PyObject *callback;
// static int interp_error;
// static int last_sequence_number;
// static bool metric;
// static double _pos_x, _pos_y, _pos_z, _pos_a, _pos_b, _pos_c, _pos_u, _pos_v, _pos_w;
// EmcPose tool_offset;

// static Interp interp_new;

// #define callmethod(o, m, f, ...) PyObject_CallMethod((o), (char*)(m), (char*)(f), ## __VA_ARGS__)

// static void maybe_new_line(int sequence_number=interp_new.sequence_number());
// static void maybe_new_line(int sequence_number) {
//     if(interp_error) return;
//     if(sequence_number == last_sequence_number)
//         return;
//     LineCode *new_line_code =
//         (LineCode*)(PyObject_New(LineCode, &LineCodeType));
//     interp_new.active_settings(new_line_code->settings);
//     interp_new.active_g_codes(new_line_code->gcodes);
//     interp_new.active_m_codes(new_line_code->mcodes);
//     new_line_code->gcodes[0] = sequence_number;
//     last_sequence_number = sequence_number;
//     PyObject *result =
//         callmethod(callback, "next_line", "O", new_line_code);
//     Py_DECREF(new_line_code);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void NURBS_FEED(int line_number, std::vector<CONTROL_POINT> nurbs_control_points, unsigned int k) {
//     double u = 0.0;
//     unsigned int n = nurbs_control_points.size() - 1;
//     double umax = n - k + 2;
//     unsigned int div = nurbs_control_points.size()*15;
//     std::vector<unsigned int> knot_vector = knot_vector_creator(n, k);
//     PLANE_POINT P1;
//     while (u+umax/div < umax) {
//         PLANE_POINT P1 = nurbs_point(u+umax/div,k,nurbs_control_points,knot_vector);
//         STRAIGHT_FEED(line_number, P1.X,P1.Y, _pos_z, _pos_a, _pos_b, _pos_c, _pos_u, _pos_v, _pos_w);
//         u = u + umax/div;
//     }
//     P1.X = nurbs_control_points[n].X;
//     P1.Y = nurbs_control_points[n].Y;
//     STRAIGHT_FEED(line_number, P1.X,P1.Y, _pos_z, _pos_a, _pos_b, _pos_c, _pos_u, _pos_v, _pos_w);
//     knot_vector.clear();
// }

// void ARC_FEED(int line_number,
//               double first_end, double second_end, double first_axis,
//               double second_axis, int rotation, double axis_end_point,
//               double a_position, double b_position, double c_position,
//               double u_position, double v_position, double w_position) {
//     // XXX: set _pos_*
//     if(metric) {
//         first_end /= 25.4;
//         second_end /= 25.4;
//         first_axis /= 25.4;
//         second_axis /= 25.4;
//         axis_end_point /= 25.4;
//         u_position /= 25.4;
//         v_position /= 25.4;
//         w_position /= 25.4;
//     }
//     maybe_new_line(line_number);
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "arc_feed", "ffffifffffff",
//                             first_end, second_end, first_axis, second_axis,
//                             rotation, axis_end_point,
//                             a_position, b_position, c_position,
//                             u_position, v_position, w_position);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void STRAIGHT_FEED(int line_number,
//                    double x, double y, double z,
//                    double a, double b, double c,
//                    double u, double v, double w) {
//     _pos_x=x; _pos_y=y; _pos_z=z;
//     _pos_a=a; _pos_b=b; _pos_c=c;
//     _pos_u=u; _pos_v=v; _pos_w=w;
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; u /= 25.4; v /= 25.4; w /= 25.4; }
//     maybe_new_line(line_number);
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "straight_feed", "fffffffff",
//                             x, y, z, a, b, c, u, v, w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void STRAIGHT_TRAVERSE(int line_number,
//                        double x, double y, double z,
//                        double a, double b, double c,
//                        double u, double v, double w) {
//     _pos_x=x; _pos_y=y; _pos_z=z;
//     _pos_a=a; _pos_b=b; _pos_c=c;
//     _pos_u=u; _pos_v=v; _pos_w=w;
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; u /= 25.4; v /= 25.4; w /= 25.4; }
//     maybe_new_line(line_number);
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "straight_traverse", "fffffffff",
//                             x, y, z, a, b, c, u, v, w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_G5X_OFFSET(int g5x_index,
//                     double x, double y, double z,
//                     double a, double b, double c,
//                     double u, double v, double w) {
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; u /= 25.4; v /= 25.4; w /= 25.4; }
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_g5x_offset", "ifffffffff",
//                             g5x_index, x, y, z, a, b, c, u, v, w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_G92_OFFSET(double x, double y, double z,
//                     double a, double b, double c,
//                     double u, double v, double w) {
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; u /= 25.4; v /= 25.4; w /= 25.4; }
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_g92_offset", "fffffffff",
//                             x, y, z, a, b, c, u, v, w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_XY_ROTATION(double t) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_xy_rotation", "f", t);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// };

// void USE_LENGTH_UNITS(CANON_UNITS u) { metric = u == CANON_UNITS_MM; }

// void SELECT_PLANE(CANON_PLANE pl) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_plane", "i", pl);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_TRAVERSE_RATE(double rate) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_traverse_rate", "f", rate);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_FEED_MODE(int mode) {
// #if 0
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "set_feed_mode", "i", mode);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// #endif
// }

// void CHANGE_TOOL(int pocket) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "change_tool", "i", pocket);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void CHANGE_TOOL_NUMBER(int pocket) {
//     maybe_new_line();
//     if(interp_error) return;
// }

// /* XXX: This needs to be re-thought.  Sometimes feed rate is not in linear
//  * units--e.g., it could be inverse time feed mode.  in that case, it's wrong
//  * to convert from mm to inch here.  but the gcode time estimate gets inverse
//  * time feed wrong anyway..
//  */
// void SET_FEED_RATE(double rate) {
//     maybe_new_line();
//     if(interp_error) return;
//     if(metric) rate /= 25.4;
//     PyObject *result =
//         callmethod(callback, "set_feed_rate", "f", rate);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void DWELL(double time) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "dwell", "f", time);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void MESSAGE(char *comment) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "message", "s", comment);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void LOG(char *s) {}
// void LOGOPEN(char *f) {}
// void LOGAPPEND(char *f) {}
// void LOGCLOSE() {}

// void COMMENT(const char *comment) {
//     maybe_new_line();
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "comment", "s", comment);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_TOOL_TABLE_ENTRY(int pocket, int toolno, EmcPose offset, double diameter,
//                           double frontangle, double backangle, int orientation) {
// }

// void USE_TOOL_LENGTH_OFFSET(EmcPose offset) {
//     tool_offset = offset;
//     maybe_new_line();
//     if(interp_error) return;
//     if(metric) {
//         offset.tran.x /= 25.4; offset.tran.y /= 25.4; offset.tran.z /= 25.4;
//         offset.u /= 25.4; offset.v /= 25.4; offset.w /= 25.4; }
//     PyObject *result = callmethod(callback, "tool_offset", "ddddddddd", offset.tran.x, offset.tran.y, offset.tran.z,
//         offset.a, offset.b, offset.c, offset.u, offset.v, offset.w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }

// void SET_FEED_REFERENCE(double reference) { }
// void SET_CUTTER_RADIUS_COMPENSATION(double radius) {}
// void START_CUTTER_RADIUS_COMPENSATION(int direction) {}
// void STOP_CUTTER_RADIUS_COMPENSATION(int direction) {}
// void START_SPEED_FEED_SYNCH() {}
// void START_SPEED_FEED_SYNCH(double sync, bool vel) {}
// void STOP_SPEED_FEED_SYNCH() {}
// void START_SPINDLE_COUNTERCLOCKWISE() {}
// void START_SPINDLE_CLOCKWISE() {}
// void SET_SPINDLE_MODE(double) {}
// void STOP_SPINDLE_TURNING() {}
// void SET_SPINDLE_SPEED(double rpm) {}
// void ORIENT_SPINDLE(double d, int i) {}
// void PROGRAM_STOP() {}
// void PROGRAM_END() {}
// void FINISH() {}
// void PALLET_SHUTTLE() {}
// void SELECT_POCKET(int tool) {}
// void OPTIONAL_PROGRAM_STOP() {}
// void START_CHANGE() {}
// int  GET_EXTERNAL_TC_FAULT() {return 0;}
// int  GET_EXTERNAL_TC_REASON() {return 0;}


// extern bool GET_BLOCK_DELETE(void) {
//     int bd = 0;
//     if(interp_error) return 0;
//     PyObject *result =
//         callmethod(callback, "get_block_delete", "");
//     if(result == NULL) {
//         interp_error++;
//     } else {
//         bd = PyObject_IsTrue(result);
//     }
//     Py_XDECREF(result);
//     return bd;
// }

// void DISABLE_FEED_OVERRIDE() {}
// void DISABLE_FEED_HOLD() {}
// void ENABLE_FEED_HOLD() {}
// void DISABLE_SPEED_OVERRIDE() {}
// void ENABLE_FEED_OVERRIDE() {}
// void ENABLE_SPEED_OVERRIDE() {}
// void MIST_OFF() {}
// void FLOOD_OFF() {}
// void MIST_ON() {}
// void FLOOD_ON() {}
// void CLEAR_AUX_OUTPUT_BIT(int bit) {}
// void SET_AUX_OUTPUT_BIT(int bit) {}
// void SET_AUX_OUTPUT_VALUE(int index, double value) {}
// void CLEAR_MOTION_OUTPUT_BIT(int bit) {}
// void SET_MOTION_OUTPUT_BIT(int bit) {}
// void SET_MOTION_OUTPUT_VALUE(int index, double value) {}
// void TURN_PROBE_ON() {}
// void TURN_PROBE_OFF() {}
// int UNLOCK_ROTARY(int line_no, int axis) {return 0;}
// int LOCK_ROTARY(int line_no, int axis) {return 0;}
// void SEND_HANDLER_ABORT(int reason) {}
// void CANON_ERROR(const char *fmt, ...) {}

// void STRAIGHT_PROBE(int line_number,
//                     double x, double y, double z,
//                     double a, double b, double c,
//                     double u, double v, double w, unsigned char probe_type) {
//     _pos_x=x; _pos_y=y; _pos_z=z;
//     _pos_a=a; _pos_b=b; _pos_c=c;
//     _pos_u=u; _pos_v=v; _pos_w=w;
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; u /= 25.4; v /= 25.4; w /= 25.4; }
//     maybe_new_line(line_number);
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "straight_probe", "fffffffff",
//                             x, y, z, a, b, c, u, v, w);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);

// }
// void RIGID_TAP(int line_number,
//                double x, double y, double z) {
//     if(metric) { x /= 25.4; y /= 25.4; z /= 25.4; }
//     maybe_new_line(line_number);
//     if(interp_error) return;
//     PyObject *result =
//         callmethod(callback, "rigid_tap", "fff",
//             x, y, z);
//     if(result == NULL) interp_error ++;
//     Py_XDECREF(result);
// }
// double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() { return 0.1; }
// double GET_EXTERNAL_PROBE_POSITION_X() { return _pos_x; }
// double GET_EXTERNAL_PROBE_POSITION_Y() { return _pos_y; }
// double GET_EXTERNAL_PROBE_POSITION_Z() { return _pos_z; }
// double GET_EXTERNAL_PROBE_POSITION_A() { return _pos_a; }
// double GET_EXTERNAL_PROBE_POSITION_B() { return _pos_b; }
// double GET_EXTERNAL_PROBE_POSITION_C() { return _pos_c; }
// double GET_EXTERNAL_PROBE_POSITION_U() { return _pos_u; }
// double GET_EXTERNAL_PROBE_POSITION_V() { return _pos_v; }
// double GET_EXTERNAL_PROBE_POSITION_W() { return _pos_w; }
// double GET_EXTERNAL_PROBE_VALUE() { return 0.0; }
// int GET_EXTERNAL_PROBE_TRIPPED_VALUE() { return 0; }
// double GET_EXTERNAL_POSITION_X() { return _pos_x; }
// double GET_EXTERNAL_POSITION_Y() { return _pos_y; }
// double GET_EXTERNAL_POSITION_Z() { return _pos_z; }
// double GET_EXTERNAL_POSITION_A() { return _pos_a; }
// double GET_EXTERNAL_POSITION_B() { return _pos_b; }
// double GET_EXTERNAL_POSITION_C() { return _pos_c; }
// double GET_EXTERNAL_POSITION_U() { return _pos_u; }
// double GET_EXTERNAL_POSITION_V() { return _pos_v; }
// double GET_EXTERNAL_POSITION_W() { return _pos_w; }
// void INIT_CANON() {}
// void GET_EXTERNAL_PARAMETER_FILE_NAME(char *name, int max_size) {
//     PyObject *result = PyObject_GetAttrString(callback, "parameter_file");
//     if(!result) { name[0] = 0; return; }
//     char *s = PyString_AsString(result);
//     if(!s) { name[0] = 0; return; }
//     memset(name, 0, max_size);
//     strncpy(name, s, max_size - 1);
// }
// int GET_EXTERNAL_LENGTH_UNIT_TYPE() { return CANON_UNITS_INCHES; }
// CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket) {
//     CANON_TOOL_TABLE t = {-1,{{0,0,0},0,0,0,0,0,0},0,0,0,0};
//     if(interp_error) return t;
//     PyObject *result =
//         callmethod(callback, "get_tool", "i", pocket);
//     if(result == NULL ||
//        !PyArg_ParseTuple(result, "iddddddddddddi", &t.toolno, &t.offset.tran.x, &t.offset.tran.y, &t.offset.tran.z,
//                           &t.offset.a, &t.offset.b, &t.offset.c, &t.offset.u, &t.offset.v, &t.offset.w,
//                           &t.diameter, &t.frontangle, &t.backangle, &t.orientation))
//             interp_error ++;

//     Py_XDECREF(result);
//     return t;
// }

// int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) { return def; }
// double GET_EXTERNAL_ANALOG_INPUT(int index, double def) { return def; }
// int WAIT(int index, int input_type, int wait_type, double timeout) { return 0;}

// static void user_defined_function(int num, double arg1, double arg2) {
//     if(interp_error) return;
//     maybe_new_line();
//     PyObject *result =
//         callmethod(callback, "user_defined_function",
//                             "idd", num, arg1, arg2);
//     if(result == NULL) interp_error++;
//     Py_XDECREF(result);
// }

// void SET_FEED_REFERENCE(int ref) {}
// int GET_EXTERNAL_QUEUE_EMPTY() { return true; }
// CANON_DIRECTION GET_EXTERNAL_SPINDLE() { return 0; }
// int GET_EXTERNAL_TOOL_SLOT() { return 0; }
// int GET_EXTERNAL_SELECTED_TOOL_SLOT() { return 0; }
// double GET_EXTERNAL_FEED_RATE() { return 1; }
// double GET_EXTERNAL_TRAVERSE_RATE() { return 0; }
// int GET_EXTERNAL_FLOOD() { return 0; }
// int GET_EXTERNAL_MIST() { return 0; }
// CANON_PLANE GET_EXTERNAL_PLANE() { return 1; }
// double GET_EXTERNAL_SPEED() { return 0; }
// int GET_EXTERNAL_POCKETS_MAX() { return CANON_POCKETS_MAX; }
// void DISABLE_ADAPTIVE_FEED() {}
// void ENABLE_ADAPTIVE_FEED() {}

// int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() {return 1;}
// int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE() {return 1;}
// int GET_EXTERNAL_ADAPTIVE_FEED_ENABLE() {return 0;}
// int GET_EXTERNAL_FEED_HOLD_ENABLE() {return 1;}

// int GET_EXTERNAL_AXIS_MASK() {
//     if(interp_error) return 7;
//     PyObject *result =
//         callmethod(callback, "get_axis_mask", "");
//     if(!result) { interp_error ++; return 7 /* XYZABC */; }
//     if(!PyInt_Check(result)) { interp_error ++; return 7 /* XYZABC */; }
//     int mask = PyInt_AsLong(result);
//     Py_DECREF(result);
//     return mask;
// }

// double GET_EXTERNAL_TOOL_LENGTH_XOFFSET() {
//     return tool_offset.tran.x;
// }
// double GET_EXTERNAL_TOOL_LENGTH_YOFFSET() {
//     return tool_offset.tran.y;
// }
// double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET() {
//     return tool_offset.tran.z;
// }
// double GET_EXTERNAL_TOOL_LENGTH_AOFFSET() {
//     return tool_offset.a;
// }
// double GET_EXTERNAL_TOOL_LENGTH_BOFFSET() {
//     return tool_offset.b;
// }
// double GET_EXTERNAL_TOOL_LENGTH_COFFSET() {
//     return tool_offset.c;
// }
// double GET_EXTERNAL_TOOL_LENGTH_UOFFSET() {
//     return tool_offset.u;
// }
// double GET_EXTERNAL_TOOL_LENGTH_VOFFSET() {
//     return tool_offset.v;
// }
// double GET_EXTERNAL_TOOL_LENGTH_WOFFSET() {
//     return tool_offset.w;
// }

// static bool PyInt_CheckAndError(const char *func, PyObject *p)  {
//     if(PyInt_Check(p)) return true;
//     PyErr_Format(PyExc_TypeError,
//             "%s: Expected int, got %s", func, p->ob_type->tp_name);
//     return false;
// }

// static bool PyFloat_CheckAndError(const char *func, PyObject *p)  {
//     if(PyFloat_Check(p)) return true;
//     PyErr_Format(PyExc_TypeError,
//             "%s: Expected float, got %s", func, p->ob_type->tp_name);
//     return false;
// }

// double GET_EXTERNAL_ANGLE_UNITS() {
//     PyObject *result =
//         callmethod(callback, "get_external_angular_units", "");
//     if(result == NULL) interp_error++;

//     double dresult = 1.0;
//     if(!result || !PyFloat_CheckAndError("get_external_angle_units", result)) {
//         interp_error++;
//     } else {
//         dresult = PyFloat_AsDouble(result);
//     }
//     Py_XDECREF(result);
//     return dresult;
// }

// double GET_EXTERNAL_LENGTH_UNITS() {
//     PyObject *result =
//         callmethod(callback, "get_external_length_units", "");
//     if(result == NULL) interp_error++;

//     double dresult = 0.03937007874016;
//     if(!result || !PyFloat_CheckAndError("get_external_length_units", result)) {
//         interp_error++;
//     } else {
//         dresult = PyFloat_AsDouble(result);
//     }
//     Py_XDECREF(result);
//     return dresult;
// }

// static bool check_abort() {
//     PyObject *result =
//         callmethod(callback, "check_abort", "");
//     if(!result) return 1;
//     if(PyObject_IsTrue(result)) {
//         Py_DECREF(result);
//         PyErr_Format(PyExc_KeyboardInterrupt, "Load aborted");
//         return 1;
//     }
//     Py_DECREF(result);
//     return 0;
// }

// USER_DEFINED_FUNCTION_TYPE USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM];

// CANON_MOTION_MODE motion_mode;
// void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance) { motion_mode = mode; }
// void SET_MOTION_CONTROL_MODE(double tolerance) { }
// void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode) { motion_mode = mode; }
// CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE() { return motion_mode; }
// void SET_NAIVECAM_TOLERANCE(double tolerance) { }

// #define RESULT_OK (result == INTERP_OK || result == INTERP_EXECUTE_FINISH)
// static PyObject *parse_file(PyObject *self, PyObject *args) {
//     char *f;
//     char *unitcode=0, *initcode=0;
//     int error_line_offset = 0;
//     struct timeval t0, t1;
//     int wait = 1;
//     if(!PyArg_ParseTuple(args, "sO|ss", &f, &callback, &unitcode, &initcode))
//         return NULL;

//     for(int i=0; i<USER_DEFINED_FUNCTION_NUM; i++)
//         USER_DEFINED_FUNCTION[i] = user_defined_function;

//     gettimeofday(&t0, NULL);

//     metric=false;
//     interp_error = 0;
//     last_sequence_number = -1;

//     _pos_x = _pos_y = _pos_z = _pos_a = _pos_b = _pos_c = 0;
//     _pos_u = _pos_v = _pos_w = 0;

//     interp_new.init();
//     interp_new.open(f);

//     maybe_new_line();

//     int result = INTERP_OK;
//     if(unitcode) {
//         result = interp_new.read(unitcode);
//         if(!RESULT_OK) goto out_error;
//         result = interp_new.execute();
//     }
//     if(initcode && RESULT_OK) {
//         result = interp_new.read(initcode);
//         if(!RESULT_OK) goto out_error;
//         result = interp_new.execute();
//     }
//     while(!interp_error && RESULT_OK) {
//         error_line_offset = 1;
//         result = interp_new.read();
//         gettimeofday(&t1, NULL);
//         if(t1.tv_sec > t0.tv_sec + wait) {
//             if(check_abort()) return NULL;
//             t0 = t1;
//         }
//         if(!RESULT_OK) break;
//         error_line_offset = 0;
//         result = interp_new.execute();
//     }
// out_error:
//     interp_new.close();
//     if(interp_error) {
//         if(!PyErr_Occurred()) {
//             PyErr_Format(PyExc_RuntimeError,
//                     "interp_error > 0 but no Python exception set");
//         }
//         return NULL;
//     }
//     PyErr_Clear();
//     maybe_new_line();
//     if(PyErr_Occurred()) { interp_error = 1; goto out_error; }
//     PyObject *retval = PyTuple_New(2);
//     PyTuple_SetItem(retval, 0, PyInt_FromLong(result));
//     PyTuple_SetItem(retval, 1, PyInt_FromLong(last_sequence_number + error_line_offset));
//     return retval;
// }


// static int maxerror = -1;

// static char savedError[LINELEN+1];
// static PyObject *rs274_strerror(PyObject *s, PyObject *o) {
//     int err;
//     if(!PyArg_ParseTuple(o, "i", &err)) return NULL;
//     interp_new.error_text(err, savedError, LINELEN);
//     return PyString_FromString(savedError);
// }

// static PyObject *rs274_calc_extents(PyObject *self, PyObject *args) {
//     double min_x = 9e99, min_y = 9e99, min_z = 9e99,
//            min_xt = 9e99, min_yt = 9e99, min_zt = 9e99,
//            max_x = -9e99, max_y = -9e99, max_z = -9e99,
//            max_xt = -9e99, max_yt = -9e99, max_zt = -9e99;
//     for(int i=0; i<PySequence_Length(args); i++) {
//         PyObject *si = PyTuple_GetItem(args, i);
//         if(!si) return NULL;
//         int j;
//         double xs, ys, zs, xe, ye, ze, xt, yt, zt;
//         for(j=0; j<PySequence_Length(si); j++) {
//             PyObject *sj = PySequence_GetItem(si, j);
//             PyObject *unused;
//             int r;
//             if(PyTuple_Size(sj) == 4)
//                 r = PyArg_ParseTuple(sj,
//                     "O(dddOOOOOO)(dddOOOOOO)(ddd):calc_extents item",
//                     &unused,
//                     &xs, &ys, &zs, &unused, &unused, &unused, &unused, &unused, &unused,
//                     &xe, &ye, &ze, &unused, &unused, &unused, &unused, &unused, &unused,
//                     &unused, &xt, &yt, &zt);
//             else
//                 r = PyArg_ParseTuple(sj,
//                     "O(dddOOOOOO)(dddOOOOOO)O(ddd):calc_extents item",
//                     &unused,
//                     &xs, &ys, &zs, &unused, &unused, &unused, &unused, &unused, &unused,
//                     &xe, &ye, &ze, &unused, &unused, &unused, &unused, &unused, &unused,
//                     &unused, &xt, &yt, &zt);
//             Py_DECREF(sj);
//             if(!r) return NULL;
//             max_x = std::max(max_x, xs);
//             max_y = std::max(max_y, ys);
//             max_z = std::max(max_z, zs);
//             min_x = std::min(min_x, xs);
//             min_y = std::min(min_y, ys);
//             min_z = std::min(min_z, zs);
//             max_xt = std::max(max_xt, xs+xt);
//             max_yt = std::max(max_yt, ys+yt);
//             max_zt = std::max(max_zt, zs+zt);
//             min_xt = std::min(min_xt, xs+xt);
//             min_yt = std::min(min_yt, ys+yt);
//             min_zt = std::min(min_zt, zs+zt);
//         }
//         if(j > 0) {
//             max_x = std::max(max_x, xe);
//             max_y = std::max(max_y, ye);
//             max_z = std::max(max_z, ze);
//             min_x = std::min(min_x, xe);
//             min_y = std::min(min_y, ye);
//             min_z = std::min(min_z, ze);
//             max_xt = std::max(max_xt, xe+xt);
//             max_yt = std::max(max_yt, ye+yt);
//             max_zt = std::max(max_zt, ze+zt);
//             min_xt = std::min(min_xt, xe+xt);
//             min_yt = std::min(min_yt, ye+yt);
//             min_zt = std::min(min_zt, ze+zt);
//         }
//     }
//     return Py_BuildValue("[ddd][ddd][ddd][ddd]",
//         min_x, min_y, min_z,  max_x, max_y, max_z,
//         min_xt, min_yt, min_zt,  max_xt, max_yt, max_zt);
// }

// #if PY_VERSION_HEX < 0x02050000
// #define PyObject_GetAttrString(o,s) \
//     PyObject_GetAttrString((o),const_cast<char*>((s)))
// #define PyArg_VaParse(o,f,a) \
//     PyArg_VaParse((o),const_cast<char*>((f)),(a))
// #endif

// static bool get_attr(PyObject *o, const char *attr_name, int *v) {
//     PyObject *attr = PyObject_GetAttrString(o, attr_name);
//     if(attr && PyInt_CheckAndError(attr_name, attr)) {
//         *v = PyInt_AsLong(attr);
//         Py_DECREF(attr);
//         return true;
//     }
//     Py_XDECREF(attr);
//     return false;
// }

// static bool get_attr(PyObject *o, const char *attr_name, double *v) {
//     PyObject *attr = PyObject_GetAttrString(o, attr_name);
//     if(attr && PyFloat_CheckAndError(attr_name, attr)) {
//         *v = PyFloat_AsDouble(attr);
//         Py_DECREF(attr);
//         return true;
//     }
//     Py_XDECREF(attr);
//     return false;
// }

// static bool get_attr(PyObject *o, const char *attr_name, const char *fmt, ...) {
//     bool result = false;
//     va_list ap;
//     va_start(ap, fmt);
//     PyObject *attr = PyObject_GetAttrString(o, attr_name);
//     if(attr) result = PyArg_VaParse(attr, fmt, ap);
//     va_end(ap);
//     Py_XDECREF(attr);
//     return result;
// }

// static void unrotate(double &x, double &y, double c, double s) {
//     double tx = x * c + y * s;
//     y = -x * s + y * c;
//     x = tx;
// }

// static void rotate(double &x, double &y, double c, double s) {
//     double tx = x * c - y * s;
//     y = x * s + y * c;
//     x = tx;
// }

// static PyObject *rs274_arc_to_segments(PyObject *self, PyObject *args) {
//     PyObject *canon;
//     double x1, y1, cx, cy, z1, a, b, c, u, v, w;
//     double o[9], n[9], g5xoffset[9], g92offset[9];
//     int rot, plane;
//     int X, Y, Z;
//     double rotation_cos, rotation_sin;
//     int max_segments = 128;

//     if(!PyArg_ParseTuple(args, "Oddddiddddddd|i:arcs_to_segments",
//         &canon, &x1, &y1, &cx, &cy, &rot, &z1, &a, &b, &c, &u, &v, &w, &max_segments)) return NULL;
//     if(!get_attr(canon, "lo", "ddddddddd:arcs_to_segments lo", &o[0], &o[1], &o[2],
//                     &o[3], &o[4], &o[5], &o[6], &o[7], &o[8]))
//         return NULL;
//     if(!get_attr(canon, "plane", &plane)) return NULL;
//     if(!get_attr(canon, "rotation_cos", &rotation_cos)) return NULL;
//     if(!get_attr(canon, "rotation_sin", &rotation_sin)) return NULL;
//     if(!get_attr(canon, "g5x_offset_x", &g5xoffset[0])) return NULL;
//     if(!get_attr(canon, "g5x_offset_y", &g5xoffset[1])) return NULL;
//     if(!get_attr(canon, "g5x_offset_z", &g5xoffset[2])) return NULL;
//     if(!get_attr(canon, "g5x_offset_a", &g5xoffset[3])) return NULL;
//     if(!get_attr(canon, "g5x_offset_b", &g5xoffset[4])) return NULL;
//     if(!get_attr(canon, "g5x_offset_c", &g5xoffset[5])) return NULL;
//     if(!get_attr(canon, "g5x_offset_u", &g5xoffset[6])) return NULL;
//     if(!get_attr(canon, "g5x_offset_v", &g5xoffset[7])) return NULL;
//     if(!get_attr(canon, "g5x_offset_w", &g5xoffset[8])) return NULL;
//     if(!get_attr(canon, "g92_offset_x", &g92offset[0])) return NULL;
//     if(!get_attr(canon, "g92_offset_y", &g92offset[1])) return NULL;
//     if(!get_attr(canon, "g92_offset_z", &g92offset[2])) return NULL;
//     if(!get_attr(canon, "g92_offset_a", &g92offset[3])) return NULL;
//     if(!get_attr(canon, "g92_offset_b", &g92offset[4])) return NULL;
//     if(!get_attr(canon, "g92_offset_c", &g92offset[5])) return NULL;
//     if(!get_attr(canon, "g92_offset_u", &g92offset[6])) return NULL;
//     if(!get_attr(canon, "g92_offset_v", &g92offset[7])) return NULL;
//     if(!get_attr(canon, "g92_offset_w", &g92offset[8])) return NULL;

//     if(plane == 1) {
//         X=0; Y=1; Z=2;
//     } else if(plane == 3) {
//         X=2; Y=0; Z=1;
//     } else {
//         X=1; Y=2; Z=0;
//     }
//     n[X] = x1;
//     n[Y] = y1;
//     n[Z] = z1;
//     n[3] = a;
//     n[4] = b;
//     n[5] = c;
//     n[6] = u;
//     n[7] = v;
//     n[8] = w;
//     for(int ax=0; ax<9; ax++) o[ax] -= g5xoffset[ax];
//     unrotate(o[0], o[1], rotation_cos, rotation_sin);
//     for(int ax=0; ax<9; ax++) o[ax] -= g92offset[ax];

//     double theta1 = atan2(o[Y]-cy, o[X]-cx);
//     double theta2 = atan2(n[Y]-cy, n[X]-cx);

//     if(rot < 0) {
//         while(theta2 - theta1 > -1e-12) theta2 -= 2*M_PI;
//     } else {
//         while(theta2 - theta1 < 1e-12) theta2 += 2*M_PI;
//     }

//     int steps = std::max(3, int(max_segments * fabs(theta1 - theta2) / M_PI));
//     double rsteps = 1. / steps;
//     PyObject *segs = PyList_New(steps);

//     double dtheta = theta2 - theta1;
//     double d[9] = {0, 0, 0, n[4]-o[4], n[5]-o[5], n[6]-o[6], n[7]-o[7], n[8]-o[8]};
//     d[Z] = n[Z] - o[Z];

//     double tx = o[X] - cx, ty = o[Y] - cy, dc = cos(dtheta*rsteps), ds = sin(dtheta*rsteps);
//     for(int i=0; i<steps-1; i++) {
//         double f = (i+1) * rsteps;
//         double p[9];
//         rotate(tx, ty, dc, ds);
//         p[X] = tx + cx;
//         p[Y] = ty + cy;
//         p[Z] = o[Z] + d[Z] * f;
//         p[3] = o[3] + d[3] * f;
//         p[4] = o[4] + d[4] * f;
//         p[5] = o[5] + d[5] * f;
//         p[6] = o[6] + d[6] * f;
//         p[7] = o[7] + d[7] * f;
//         p[8] = o[8] + d[8] * f;
//         for(int ax=0; ax<9; ax++) p[ax] += g92offset[ax];
//         rotate(p[0], p[1], rotation_cos, rotation_sin);
//         for(int ax=0; ax<9; ax++) p[ax] += g5xoffset[ax];
//         PyList_SET_ITEM(segs, i,
//             Py_BuildValue("ddddddddd", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]));
//     }
//     for(int ax=0; ax<9; ax++) n[ax] += g92offset[ax];
//     rotate(n[0], n[1], rotation_cos, rotation_sin);
//     for(int ax=0; ax<9; ax++) n[ax] += g5xoffset[ax];
//     PyList_SET_ITEM(segs, steps-1,
//         Py_BuildValue("ddddddddd", n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8]));
//     return segs;
// }

// static PyMethodDef gcode_methods[] = {
//     {"parse", (PyCFunction)parse_file, METH_VARARGS, "Parse a G-Code file"},
//     {"strerror", (PyCFunction)rs274_strerror, METH_VARARGS,
//         "Convert a numeric error to a string"},
//     {"calc_extents", (PyCFunction)rs274_calc_extents, METH_VARARGS,
//         "Calculate information about extents of gcode"},
//     {"arc_to_segments", (PyCFunction)rs274_arc_to_segments, METH_VARARGS,
//         "Convert an arc to straight segments"},
//     {NULL}
// };

// PyMODINIT_FUNC
// initgcode(void) {
//     PyObject *m = Py_InitModule3("gcode", gcode_methods,
//                 "Interface to EMC rs274ngc interpreter");
//     PyType_Ready(&LineCodeType);
//     PyModule_AddObject(m, "linecode", (PyObject*)&LineCodeType);
//     PyObject_SetAttrString(m, "MAX_ERROR", PyInt_FromLong(maxerror));
//     PyObject_SetAttrString(m, "MIN_ERROR",
//             PyInt_FromLong(INTERP_MIN_ERROR));
// }

// // vim:ts=8:sts=4:sw=4:et:
//    This is a component of AXIS, a front-end for emc
//    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
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

// #include <Python.h>
// #include <structseq.h>
// #include <pthread.h>
// #include <structmember.h>
// #include "config.h"
// #include "rcs.hh"
// #include "emc.hh"
// #include "emc_nml.hh"
// #include "kinematics.h"
// #include "config.h"
// #include "inifile.hh"
// #include "timer.hh"
// #include "nml_oi.hh"
// #include "rcs_print.hh"

// #include <cmath>

// #ifndef T_BOOL
// // The C++ standard probably doesn't specify the amount of storage for a 'bool',
// // and on some systems it might be more than one byte.  However, on x86 and
// // x86-64, sizeof(bool) == 1.  When a counterexample is found, this must be
// // replaced with the result of a configure test.
// #define T_BOOL T_UBYTE
// #endif

// #define NUM_AXES (9)

// #define LOCAL_SPINDLE_FORWARD (1)
// #define LOCAL_SPINDLE_REVERSE (-1)
// #define LOCAL_SPINDLE_OFF (0)
// #define LOCAL_SPINDLE_INCREASE (10)
// #define LOCAL_SPINDLE_DECREASE (11)
// #define LOCAL_SPINDLE_CONSTANT (12)

// #define LOCAL_MIST_ON (1)
// #define LOCAL_MIST_OFF (0)

// #define LOCAL_FLOOD_ON (1)
// #define LOCAL_FLOOD_OFF (0)

// #define LOCAL_BRAKE_ENGAGE (1)
// #define LOCAL_BRAKE_RELEASE (0)

// #define LOCAL_JOG_STOP (0)
// #define LOCAL_JOG_CONTINUOUS (1)
// #define LOCAL_JOG_INCREMENT (2)

// #define LOCAL_AUTO_RUN (0)
// #define LOCAL_AUTO_PAUSE (1)
// #define LOCAL_AUTO_RESUME (2)
// #define LOCAL_AUTO_STEP (3)

// /* This definition of offsetof avoids the g++ warning
//  * 'invalid offsetof from non-POD type'.
//  */
// #undef offsetof
// #define offsetof(T,x) (size_t)(-1+(char*)&(((T*)1)->x))

// struct pyIniFile {
//     PyObject_HEAD
//     IniFile *i;
// };

// struct pyStatChannel {
//     PyObject_HEAD
//     RCS_STAT_CHANNEL *c;
//     EMC_STAT status;
// };

// struct pyCommandChannel {
//     PyObject_HEAD
//     RCS_CMD_CHANNEL *c;
//     RCS_STAT_CHANNEL *s;
//     int serial;
// };

// struct pyErrorChannel {
//     PyObject_HEAD
//     NML *c;
// };

// static PyObject *m = NULL, *error = NULL;

// static int Ini_init(pyIniFile *self, PyObject *a, PyObject *k) {
//     char *inifile;
//     if(!PyArg_ParseTuple(a, "s", &inifile)) return -1;

//     self->i = new IniFile();

//     if (!self->i->Open(inifile)) {
//         PyErr_Format( error, "inifile.open() failed");
//         return -1;
//     }
//     return 0;
// }

// static PyObject *Ini_find(pyIniFile *self, PyObject *args) {
//     const char *s1, *s2, *out;
//     int num = 1;
//     if(!PyArg_ParseTuple(args, "ss|i:find", &s1, &s2, &num)) return NULL;

//     out = self->i->Find(s2, s1, num);
//     if(out == NULL) {
//         Py_INCREF(Py_None);
//         return Py_None;
//     }
//     return PyString_FromString(const_cast<char*>(out));
// }

// static PyObject *Ini_findall(pyIniFile *self, PyObject *args) {
//     const char *s1, *s2, *out;
//     int num = 1;
//     if(!PyArg_ParseTuple(args, "ss:findall", &s1, &s2)) return NULL;

//     PyObject *result = PyList_New(0);
//     while(1) {
//         out = self->i->Find(s2, s1, num);
//         if(out == NULL) {
//             break;
//         }
//         PyList_Append(result, PyString_FromString(const_cast<char*>(out)));
//         num++;
//     }
//     return result;
// }

// static void Ini_dealloc(pyIniFile *self) {
//     self->i->Close();
//     delete self->i;
//     PyObject_Del(self);
// }

// static PyMethodDef Ini_methods[] = {
//     {"find", (PyCFunction)Ini_find, METH_VARARGS,
//         "Find value in inifile as string.  This uses the ConfigParser-style "
//         "(section,option) order, not the emc order."},
//     {"findall", (PyCFunction)Ini_findall, METH_VARARGS,
//         "Find value in inifile as a list.  This uses the ConfigParser-style "
//         "(section,option) order, not the emc order."},
//     {NULL}
// };

// static PyTypeObject Ini_Type = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "emc.ini",              /*tp_name*/
//     sizeof(pyIniFile),      /*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     (destructor)Ini_dealloc,/*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     Ini_methods,            /*tp_methods*/
//     0,                      /*tp_members*/
//     0,                      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     (initproc)Ini_init,     /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// #define EMC_COMMAND_TIMEOUT 5.0  // how long to wait until timeout
// #define EMC_COMMAND_DELAY   0.01 // how long to sleep between checks

// static int emcWaitCommandComplete(int serial_number, RCS_STAT_CHANNEL *s, double timeout) {
//     double start = etime();

//     do {
//         double now = etime();
//         if(s->peek() == EMC_STAT_TYPE) {
//            EMC_STAT *stat = (EMC_STAT*)s->get_address();
// //           printf("WaitComplete: %d %d %d\n", serial_number, stat->echo_serial_number, stat->status);
//            if (stat->echo_serial_number == serial_number &&
//                ( stat->status == RCS_DONE || stat->status == RCS_ERROR )) {
//                 return s->get_address()->status;
//            }
//         }
//         esleep(fmin(timeout - (now - start), EMC_COMMAND_DELAY));
//     } while (etime() - start < timeout);
//     return -1;
// }

// static void emcWaitCommandReceived(int serial_number, RCS_STAT_CHANNEL *s) {
//     double start = etime();

//     while (etime() - start < EMC_COMMAND_TIMEOUT) {
//         if(s->peek() == EMC_STAT_TYPE &&
//            s->get_address()->echo_serial_number == serial_number) {
//                 return;
//            }
//         esleep(EMC_COMMAND_DELAY);
//     }
// }

// static int next_serial(pyCommandChannel *c) {
//     return ++c->serial;
// }

// static char *get_nmlfile(void) {
//     PyObject *fileobj = PyObject_GetAttrString(m, "nmlfile");
//     if(fileobj == NULL) return NULL;
//     return PyString_AsString(fileobj);
// }

// static int Stat_init(pyStatChannel *self, PyObject *a, PyObject *k) {
//     char *file = get_nmlfile();
//     if(file == NULL) return -1;

//     RCS_STAT_CHANNEL *c =
//         new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", file);
//     if(!c) {
//         PyErr_Format( error, "new RCS_STAT_CHANNEL failed");
//         return -1;
//     }

//     self->c = c;
//     return 0;
// }

// static void Stat_dealloc(PyObject *self) {
//     delete ((pyStatChannel*)self)->c;
//     PyObject_Del(self);
// }

// static bool check_stat(RCS_STAT_CHANNEL *emcStatusBuffer) {
//     if(!emcStatusBuffer->valid()) {
//         PyErr_Format( error, "emcStatusBuffer invalid err=%d", emcStatusBuffer->error_type);
//         return false;
//     }
//     return true;
// }

// static PyObject *poll(pyStatChannel *s, PyObject *o) {
//     if(!check_stat(s->c)) return NULL;
//     if(s->c->peek() == EMC_STAT_TYPE) {
//         EMC_STAT *emcStatus = static_cast<EMC_STAT*>(s->c->get_address());
//         memcpy(&s->status, emcStatus, sizeof(EMC_STAT));
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyMethodDef Stat_methods[] = {
//     {"poll", (PyCFunction)poll, METH_NOARGS, "Update current machine state"},
//     {NULL}
// };

// #define O(x) offsetof(pyStatChannel,status.x)
// static PyMemberDef Stat_members[] = {
// // stat
//     {(char*)"echo_serial_number", T_INT, O(echo_serial_number), READONLY},
//     {(char*)"state", T_INT, O(status), READONLY},

// // task
//     {(char*)"task_mode", T_INT, O(task.mode), READONLY},
//     {(char*)"task_state", T_INT, O(task.state), READONLY},
//     {(char*)"exec_state", T_INT, O(task.execState), READONLY},
//     {(char*)"interp_state", T_INT, O(task.interpState), READONLY},
//     {(char*)"read_line", T_INT, O(task.readLine), READONLY},
//     {(char*)"motion_line", T_INT, O(task.motionLine), READONLY},
//     {(char*)"current_line", T_INT, O(task.currentLine), READONLY},
//     {(char*)"file", T_STRING_INPLACE, O(task.file), READONLY},
//     {(char*)"command", T_STRING_INPLACE, O(task.command), READONLY},
//     {(char*)"program_units", T_INT, O(task.programUnits), READONLY},
//     {(char*)"interpreter_errcode", T_INT, O(task.interpreter_errcode), READONLY},
//     {(char*)"optional_stop", T_BOOL, O(task.optional_stop_state), READONLY},
//     {(char*)"block_delete", T_BOOL, O(task.block_delete_state), READONLY},
//     {(char*)"task_paused", T_INT, O(task.task_paused), READONLY},
//     {(char*)"input_timeout", T_BOOL, O(task.input_timeout), READONLY},
//     {(char*)"rotation_xy", T_DOUBLE, O(task.rotation_xy), READONLY},
//     {(char*)"delay_left", T_DOUBLE, O(task.delayLeft), READONLY},

// // motion
// //   EMC_TRAJ_STAT traj
//     {(char*)"linear_units", T_DOUBLE, O(motion.traj.linearUnits), READONLY},
//     {(char*)"angular_units", T_DOUBLE, O(motion.traj.angularUnits), READONLY},
//     {(char*)"cycle_time", T_DOUBLE, O(motion.traj.cycleTime), READONLY},
//     {(char*)"axes", T_INT, O(motion.traj.axes), READONLY},
//     {(char*)"axis_mask", T_INT, O(motion.traj.axis_mask), READONLY},
//     {(char*)"motion_mode", T_INT, O(motion.traj.mode), READONLY},
//     {(char*)"enabled", T_BOOL, O(motion.traj.enabled), READONLY},
//     {(char*)"inpos", T_BOOL, O(motion.traj.inpos), READONLY},
//     {(char*)"queue", T_INT, O(motion.traj.queue), READONLY},
//     {(char*)"active_queue", T_INT, O(motion.traj.activeQueue), READONLY},
//     {(char*)"queue_full", T_BOOL, O(motion.traj.queueFull), READONLY},
//     {(char*)"id", T_INT, O(motion.traj.id), READONLY},
//     {(char*)"paused", T_BOOL, O(motion.traj.paused), READONLY},
//     {(char*)"feedrate", T_DOUBLE, O(motion.traj.scale), READONLY},
//     {(char*)"spindlerate", T_DOUBLE, O(motion.traj.spindle_scale), READONLY},

//     {(char*)"velocity", T_DOUBLE, O(motion.traj.velocity), READONLY},
//     {(char*)"acceleration", T_DOUBLE, O(motion.traj.acceleration), READONLY},
//     {(char*)"max_velocity", T_DOUBLE, O(motion.traj.maxVelocity), READONLY},
//     {(char*)"max_acceleration", T_DOUBLE, O(motion.traj.maxAcceleration), READONLY},
//     {(char*)"probe_tripped", T_BOOL, O(motion.traj.probe_tripped), READONLY},
//     {(char*)"probing", T_BOOL, O(motion.traj.probing), READONLY},
//     {(char*)"probe_val", T_INT, O(motion.traj.probeval), READONLY},
//     {(char*)"kinematics_type", T_INT, O(motion.traj.kinematics_type), READONLY},
//     {(char*)"motion_type", T_INT, O(motion.traj.motion_type), READONLY},
//     {(char*)"distance_to_go", T_DOUBLE, O(motion.traj.distance_to_go), READONLY},
//     {(char*)"current_vel", T_DOUBLE, O(motion.traj.current_vel), READONLY},
//     {(char*)"feed_override_enabled", T_BOOL, O(motion.traj.feed_override_enabled), READONLY},
//     {(char*)"spindle_override_enabled", T_BOOL, O(motion.traj.spindle_override_enabled), READONLY},
//     {(char*)"adaptive_feed_enabled", T_BOOL, O(motion.traj.adaptive_feed_enabled), READONLY},
//     {(char*)"feed_hold_enabled", T_BOOL, O(motion.traj.feed_hold_enabled), READONLY},

// // EMC_SPINDLE_STAT motion.spindle
//     {(char*)"spindle_speed", T_DOUBLE, O(motion.spindle.speed), READONLY},
//     {(char*)"spindle_direction", T_INT, O(motion.spindle.direction), READONLY},
//     {(char*)"spindle_brake", T_INT, O(motion.spindle.brake), READONLY},
//     {(char*)"spindle_increasing", T_INT, O(motion.spindle.increasing), READONLY},
//     {(char*)"spindle_enabled", T_INT, O(motion.spindle.enabled), READONLY},

// // io
// // EMC_TOOL_STAT io.tool
//     {(char*)"pocket_prepped", T_INT, O(io.tool.pocketPrepped), READONLY},
//     {(char*)"tool_in_spindle", T_INT, O(io.tool.toolInSpindle), READONLY},

// // EMC_COOLANT_STAT io.cooland
//     {(char*)"mist", T_INT, O(io.coolant.mist), READONLY},
//     {(char*)"flood", T_INT, O(io.coolant.flood), READONLY},

// // EMC_AUX_STAT     io.aux
//     {(char*)"estop", T_INT, O(io.aux.estop), READONLY},

// // EMC_LUBE_STAT    io.lube
//     {(char*)"lube", T_INT, O(io.lube.on), READONLY},
//     {(char*)"lube_level", T_INT, O(io.lube.level), READONLY},

//     {(char*)"debug", T_INT, O(debug), READONLY},
//     {NULL}
// };

// static PyObject *int_array(int *arr, int sz) {
//     PyObject *res = PyTuple_New(sz);
//     for(int i = 0; i < sz; i++) {
//         PyTuple_SET_ITEM(res, i, PyInt_FromLong(arr[i]));
//     }
//     return res;
// }

// static PyObject *double_array(double *arr, int sz) {
//     PyObject *res = PyTuple_New(sz);
//     for(int i = 0; i < sz; i++) {
//         PyTuple_SET_ITEM(res, i, PyFloat_FromDouble(arr[i]));
//     }
//     return res;
// }

// static PyObject *pose(const EmcPose &p) {
//     PyObject *res = PyTuple_New(9);
//     PyTuple_SET_ITEM(res, 0, PyFloat_FromDouble(p.tran.x));
//     PyTuple_SET_ITEM(res, 1, PyFloat_FromDouble(p.tran.y));
//     PyTuple_SET_ITEM(res, 2, PyFloat_FromDouble(p.tran.z));
//     PyTuple_SET_ITEM(res, 3, PyFloat_FromDouble(p.a));
//     PyTuple_SET_ITEM(res, 4, PyFloat_FromDouble(p.b));
//     PyTuple_SET_ITEM(res, 5, PyFloat_FromDouble(p.c));
//     PyTuple_SET_ITEM(res, 6, PyFloat_FromDouble(p.u));
//     PyTuple_SET_ITEM(res, 7, PyFloat_FromDouble(p.v));
//     PyTuple_SET_ITEM(res, 8, PyFloat_FromDouble(p.w));
//     return res;
// }

// static PyObject *Stat_g5x_index(pyStatChannel *s) {
//     return PyInt_FromLong(s->status.task.g5x_index);
// }

// static PyObject *Stat_g5x_offset(pyStatChannel *s) {
//     return pose(s->status.task.g5x_offset);
// }

// static PyObject *Stat_g92_offset(pyStatChannel *s) {
//     return pose(s->status.task.g92_offset);
// }

// static PyObject *Stat_tool_offset(pyStatChannel *s) {
//     return pose(s->status.task.toolOffset);
// }

// static PyObject *Stat_position(pyStatChannel *s) {
//     return pose(s->status.motion.traj.position);
// }

// static PyObject *Stat_dtg(pyStatChannel *s) {
//     return pose(s->status.motion.traj.dtg);
// }

// static PyObject *Stat_actual(pyStatChannel *s) {
//     return pose(s->status.motion.traj.actualPosition);
// }

// static PyObject *Stat_joint_position(pyStatChannel *s) {
//     PyObject *res = PyTuple_New(EMC_AXIS_MAX);
//     for(int i=0; i<EMC_AXIS_MAX; i++) {
//         PyTuple_SetItem(res, i,
//                 PyFloat_FromDouble(s->status.motion.axis[i].output));
//     }
//     return res;
// }

// static PyObject *Stat_joint_actual(pyStatChannel *s) {
//     PyObject *res = PyTuple_New(EMC_AXIS_MAX);
//     for(int i=0; i<EMC_AXIS_MAX; i++) {
//         PyTuple_SetItem(res, i,
//                 PyFloat_FromDouble(s->status.motion.axis[i].input));
//     }
//     return res;
// }

// static PyObject *Stat_probed(pyStatChannel *s) {
//     return pose(s->status.motion.traj.probedPosition);
// }

// static PyObject *Stat_activegcodes(pyStatChannel *s) {
//     return int_array(s->status.task.activeGCodes, ACTIVE_G_CODES);
// }

// static PyObject *Stat_activemcodes(pyStatChannel *s) {
//     return int_array(s->status.task.activeMCodes, ACTIVE_M_CODES);
// }

// static PyObject *Stat_activesettings(pyStatChannel *s) {
//    return double_array(s->status.task.activeSettings, ACTIVE_SETTINGS);
// }

// static PyObject *Stat_din(pyStatChannel *s) {
//     return int_array(s->status.motion.synch_di, EMC_MAX_AIO);
// }

// static PyObject *Stat_dout(pyStatChannel *s) {
//     return int_array(s->status.motion.synch_do, EMC_MAX_AIO);
// }

// static PyObject *Stat_limit(pyStatChannel *s) {
//     int sz = NUM_AXES;
//     PyObject *res = PyTuple_New(sz);
//     for(int i = 0; i < sz; i++) {
//         int v = 0;
//         if(s->status.motion.axis[i].minHardLimit) v |= 1;
//         if(s->status.motion.axis[i].maxHardLimit) v |= 2;
//         if(s->status.motion.axis[i].minSoftLimit) v |= 4;
//         if(s->status.motion.axis[i].maxSoftLimit) v |= 8;
//         PyTuple_SET_ITEM(res, i, PyInt_FromLong(v));
//     }
//     return res;
// }

// static PyObject *Stat_homed(pyStatChannel *s) {
//     int sz = NUM_AXES;
//     PyObject *res = PyTuple_New(sz);
//     for(int i = 0; i < sz; i++) {
//         PyTuple_SET_ITEM(res, i, PyInt_FromLong(s->status.motion.axis[i].homed));
//     }
//     return res;
// }

// static PyObject *Stat_ain(pyStatChannel *s) {
//     return double_array(s->status.motion.analog_input, EMC_MAX_AIO);
// }

// static PyObject *Stat_aout(pyStatChannel *s) {
//     return double_array(s->status.motion.analog_output, EMC_MAX_AIO);
// }

// static void dict_add(PyObject *d, const char *name, unsigned char v) {
//     PyObject *o;
//     PyDict_SetItemString(d, name, o = PyInt_FromLong(v));
//     Py_XDECREF(o);
// }
// static void dict_add(PyObject *d, const char *name, double v) {
//     PyObject *o;
//     PyDict_SetItemString(d, name, o = PyFloat_FromDouble(v));
//     Py_XDECREF(o);
// }
// #define F(x) F2(#x, x)
// #define F2(y,x) dict_add(res, y, s->status.motion.axis[axisno].x)
// static PyObject *Stat_axis_one(pyStatChannel *s, int axisno) {
//     PyObject *res = PyDict_New();
//     F(axisType);
//     F(units);
//     F(backlash);
//     F2("min_position_limit", minPositionLimit);
//     F2("max_position_limit", maxPositionLimit);
//     F2("max_ferror", maxFerror);
//     F2("min_ferror", minFerror);
//     F2("ferror_current", ferrorCurrent);
//     F2("ferror_highmark", ferrorHighMark);
//     F(output);
//     F(input);
//     F(velocity);
//     F(inpos);
//     F(homing);
//     F(homed);
//     F(fault);
//     F(enabled);
//     F2("min_soft_limit", minSoftLimit);
//     F2("max_soft_limit", maxSoftLimit);
//     F2("min_hard_limit", minHardLimit);
//     F2("max_hard_limit", maxHardLimit);
//     F2("override_limits", overrideLimits);
//     return res;
// }
// #undef F
// #undef F2

// static PyObject *Stat_axis(pyStatChannel *s) {
//     PyObject *res = PyTuple_New(EMC_AXIS_MAX);
//     for(int i=0; i<EMC_AXIS_MAX; i++) {
//         PyTuple_SetItem(res, i, Stat_axis_one(s, i));
//     }
//     return res;
// }

// static PyStructSequence_Field tool_fields[] = {
//     {(char*)"id", },
//     {(char*)"xoffset", },
//     {(char*)"yoffset", },
//     {(char*)"zoffset", },
//     {(char*)"aoffset", },
//     {(char*)"boffset", },
//     {(char*)"coffset", },
//     {(char*)"uoffset", },
//     {(char*)"voffset", },
//     {(char*)"woffset", },
//     {(char*)"diameter", },
//     {(char*)"frontangle", },
//     {(char*)"backangle", },
//     {(char*)"orientation", },
//     {0,},
// };

// static PyStructSequence_Desc tool_result_desc = {
//     (char*)"tool_result", /* name */
//     (char*)"", /* doc */
//     tool_fields,
//     14
// };

// static PyTypeObject ToolResultType;

// static PyObject *Stat_tool_table(pyStatChannel *s) {
//     PyObject *res = PyTuple_New(CANON_POCKETS_MAX);
//     int j=0;
//     for(int i=0; i<CANON_POCKETS_MAX; i++) {
//         struct CANON_TOOL_TABLE &t = s->status.io.tool.toolTable[i];
//         PyObject *tool = PyStructSequence_New(&ToolResultType);
//         PyStructSequence_SET_ITEM(tool, 0, PyInt_FromLong(t.toolno));
//         PyStructSequence_SET_ITEM(tool, 1, PyFloat_FromDouble(t.offset.tran.x));
//         PyStructSequence_SET_ITEM(tool, 2, PyFloat_FromDouble(t.offset.tran.y));
//         PyStructSequence_SET_ITEM(tool, 3, PyFloat_FromDouble(t.offset.tran.z));
//         PyStructSequence_SET_ITEM(tool, 4, PyFloat_FromDouble(t.offset.a));
//         PyStructSequence_SET_ITEM(tool, 5, PyFloat_FromDouble(t.offset.b));
//         PyStructSequence_SET_ITEM(tool, 6, PyFloat_FromDouble(t.offset.c));
//         PyStructSequence_SET_ITEM(tool, 7, PyFloat_FromDouble(t.offset.u));
//         PyStructSequence_SET_ITEM(tool, 8, PyFloat_FromDouble(t.offset.v));
//         PyStructSequence_SET_ITEM(tool, 9, PyFloat_FromDouble(t.offset.w));
//         PyStructSequence_SET_ITEM(tool, 10, PyFloat_FromDouble(t.diameter));
//         PyStructSequence_SET_ITEM(tool, 11, PyFloat_FromDouble(t.frontangle));
//         PyStructSequence_SET_ITEM(tool, 12, PyFloat_FromDouble(t.backangle));
//         PyStructSequence_SET_ITEM(tool, 13, PyInt_FromLong(t.orientation));
//         PyTuple_SetItem(res, j, tool);
//         j++;
//     }
//     _PyTuple_Resize(&res, j);
//     return res;
// }

// // XXX io.tool.toolTable
// // XXX EMC_AXIS_STAT motion.axis[]

// static PyGetSetDef Stat_getsetlist[] = {
//     {(char*)"actual_position", (getter)Stat_actual},
//     {(char*)"ain", (getter)Stat_ain},
//     {(char*)"aout", (getter)Stat_aout},
//     {(char*)"axis", (getter)Stat_axis},
//     {(char*)"din", (getter)Stat_din},
//     {(char*)"dout", (getter)Stat_dout},
//     {(char*)"gcodes", (getter)Stat_activegcodes},
//     {(char*)"homed", (getter)Stat_homed},
//     {(char*)"limit", (getter)Stat_limit},
//     {(char*)"mcodes", (getter)Stat_activemcodes},
//     {(char*)"g5x_offset", (getter)Stat_g5x_offset},
//     {(char*)"g5x_index", (getter)Stat_g5x_index},
//     {(char*)"g92_offset", (getter)Stat_g92_offset},
//     {(char*)"position", (getter)Stat_position},
//     {(char*)"dtg", (getter)Stat_dtg},
//     {(char*)"joint_position", (getter)Stat_joint_position},
//     {(char*)"joint_actual_position", (getter)Stat_joint_actual},
//     {(char*)"probed_position", (getter)Stat_probed},
//     {(char*)"settings", (getter)Stat_activesettings},
//     {(char*)"tool_offset", (getter)Stat_tool_offset},
//     {(char*)"tool_table", (getter)Stat_tool_table},
//     {NULL}
// };

// static PyTypeObject Stat_Type = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "emc.stat",             /*tp_name*/
//     sizeof(pyStatChannel),  /*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     (destructor)Stat_dealloc, /*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     Stat_methods,           /*tp_methods*/
//     Stat_members,           /*tp_members*/
//     Stat_getsetlist,        /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     (initproc)Stat_init,    /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// static int Command_init(pyCommandChannel *self, PyObject *a, PyObject *k) {
//     char *file = get_nmlfile();
//     if(file == NULL) return -1;

//     RCS_CMD_CHANNEL *c =
//         new RCS_CMD_CHANNEL(emcFormat, "emcCommand", "xemc", file);
//     if(!c) {
//         PyErr_Format( error, "new RCS_CMD_CHANNEL failed");
//         return -1;
//     }
//     RCS_STAT_CHANNEL *s =
//         new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", file);
//     if(!c) {
// 	delete s;
//         PyErr_Format( error, "new RCS_STAT_CHANNEL failed");
//         return -1;
//     }

//     self->s = s;
//     self->c = c;
//     return 0;
// }

// static void Command_dealloc(PyObject *self) {
//     delete ((pyCommandChannel*)self)->c;
//     PyObject_Del(self);
// }

// static PyObject *block_delete(pyCommandChannel *s, PyObject *o) {
//     int t;
//     EMC_TASK_PLAN_SET_BLOCK_DELETE m;

//     if(!PyArg_ParseTuple(o, "i", &t)) return NULL;
//     m.state = t;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *optional_stop(pyCommandChannel *s, PyObject *o) {
//     int t;
//     EMC_TASK_PLAN_SET_OPTIONAL_STOP m;

//     if(!PyArg_ParseTuple(o, "i", &t)) return NULL;
//     m.state = t;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *mode(pyCommandChannel *s, PyObject *o) {
//     EMC_TASK_SET_MODE m;
//     if(!PyArg_ParseTuple(o, "i", &m.mode)) return NULL;
//     switch(m.mode) {
//         case EMC_TASK_MODE_MDI:
//         case EMC_TASK_MODE_MANUAL:
//         case EMC_TASK_MODE_AUTO:
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"Mode should be MODE_MDI, MODE_MANUAL, or MODE_AUTO");
//             return NULL;
//     }
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *maxvel(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_MAX_VELOCITY m;
//     if(!PyArg_ParseTuple(o, "d", &m.velocity)) return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *feedrate(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_SCALE m;
//     if(!PyArg_ParseTuple(o, "d", &m.scale)) return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *spindleoverride(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_SPINDLE_SCALE m;
//     if(!PyArg_ParseTuple(o, "d", &m.scale)) return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *spindle(pyCommandChannel *s, PyObject *o) {
//     int dir;
//     if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
//     switch(dir) {
//         case LOCAL_SPINDLE_FORWARD:
//         case LOCAL_SPINDLE_REVERSE:
//         {
//             EMC_SPINDLE_ON m;
//             m.speed = dir;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_SPINDLE_INCREASE:
//         {
//             EMC_SPINDLE_INCREASE m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_SPINDLE_DECREASE:
//         {
//             EMC_SPINDLE_DECREASE m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_SPINDLE_CONSTANT:
//         {
//             EMC_SPINDLE_CONSTANT m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_SPINDLE_OFF:
//         {
//             EMC_SPINDLE_OFF m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"Spindle direction should be SPINDLE_FORWARD, SPINDLE_REVERSE, SPINDLE_OFF, SPINDLE_INCREASE, SPINDLE_DECREASE, or SPINDLE_CONSTANT");
//             return NULL;
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *mdi(pyCommandChannel *s, PyObject *o) {
//     char *cmd;
//     int len;
//     if(!PyArg_ParseTuple(o, "s#", &cmd, &len)) return NULL;
//     if(len >= 255) {
//         PyErr_Format(PyExc_ValueError,"MDI commands limited to 255 characters");
//         return NULL;
//     }
//     EMC_TASK_PLAN_EXECUTE m;
//     m.serial_number = next_serial(s);
//     strcpy(m.command, cmd);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *state(pyCommandChannel *s, PyObject *o) {
//     EMC_TASK_SET_STATE m;
//     if(!PyArg_ParseTuple(o, "i", &m.state)) return NULL;
//     switch(m.state){
//         case EMC_TASK_STATE_ESTOP:
//         case EMC_TASK_STATE_ESTOP_RESET:
//         case EMC_TASK_STATE_ON:
//         case EMC_TASK_STATE_OFF:
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"Machine state should be STATE_ESTOP, STATE_ESTOP_RESET, STATE_ON, or STATE_OFF");
//             return NULL;
//     }
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *tool_offset(pyCommandChannel *s, PyObject *o) {
//     EMC_TOOL_SET_OFFSET m;
//     if(!PyArg_ParseTuple(o, "idddddi", &m.toolno, &m.offset.tran.z, &m.offset.tran.x, &m.diameter,
//                          &m.frontangle, &m.backangle, &m.orientation))
//         return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *mist(pyCommandChannel *s, PyObject *o) {
//     int dir;
//     if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
//     switch(dir) {
//         case LOCAL_MIST_ON:
//         {
//             EMC_COOLANT_MIST_ON m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_MIST_OFF:
//         {
//             EMC_COOLANT_MIST_OFF m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"Mist should be MIST_ON or MIST_OFF");
//             return NULL;
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *flood(pyCommandChannel *s, PyObject *o) {
//     int dir;
//     if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
//     switch(dir) {
//         case LOCAL_FLOOD_ON:
//         {
//             EMC_COOLANT_FLOOD_ON m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_FLOOD_OFF:
//         {
//             EMC_COOLANT_FLOOD_OFF m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"FLOOD should be FLOOD_ON or FLOOD_OFF");
//             return NULL;
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *brake(pyCommandChannel *s, PyObject *o) {
//     int dir;
//     if(!PyArg_ParseTuple(o, "i", &dir)) return NULL;
//     switch(dir) {
//         case LOCAL_BRAKE_ENGAGE:
//         {
//             EMC_SPINDLE_BRAKE_ENGAGE m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         case LOCAL_BRAKE_RELEASE:
//         {
//             EMC_SPINDLE_BRAKE_RELEASE m;
//             m.serial_number = next_serial(s);
//             s->c->write(m);
//             emcWaitCommandReceived(s->serial, s->s);
//         }
//             break;
//         default:
//             PyErr_Format(PyExc_ValueError,"BRAKE should be BRAKE_ENGAGE or BRAKE_RELEASE");
//             return NULL;
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *load_tool_table(pyCommandChannel *s, PyObject *o) {
//     EMC_TOOL_LOAD_TOOL_TABLE m;
//     m.file[0] = '\0'; // don't override the ini file
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *emcabort(pyCommandChannel *s, PyObject *o) {
//     EMC_TASK_ABORT m;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *override_limits(pyCommandChannel *s, PyObject *o) {
//     EMC_AXIS_OVERRIDE_LIMITS m;
//     m.axis = 0; // same number for all
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *home(pyCommandChannel *s, PyObject *o) {
//     EMC_AXIS_HOME m;
//     if(!PyArg_ParseTuple(o, "i", &m.axis)) return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *unhome(pyCommandChannel *s, PyObject *o) {
//     EMC_AXIS_UNHOME m;
//     if(!PyArg_ParseTuple(o, "i", &m.axis)) return NULL;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// // jog(JOG_STOP, axis)
// // jog(JOG_CONTINUOUS, axis, speed)
// // jog(JOG_INCREMENT, axis, speed, increment)

// static PyObject *jog(pyCommandChannel *s, PyObject *o) {
//     int fn;
//     int axis;
//     double vel, inc;


//     if(!PyArg_ParseTuple(o, "ii|dd", &fn, &axis, &vel, &inc)) return NULL;
//     if(fn == LOCAL_JOG_STOP) {
//         if(PyTuple_Size(o) != 2) {
//             PyErr_Format( PyExc_TypeError,
//                 "jog(JOG_STOP, ...) takes 2 arguments (%lu given)",
//                 (unsigned long)PyTuple_Size(o));
//             return NULL;
//         }
//         EMC_AXIS_ABORT abort;
//         abort.axis = axis;
//         abort.serial_number = next_serial(s);
//         s->c->write(abort);
//         emcWaitCommandReceived(s->serial, s->s);
//     } else if(fn == LOCAL_JOG_CONTINUOUS) {
//         if(PyTuple_Size(o) != 3) {
//             PyErr_Format( PyExc_TypeError,
//                 "jog(JOG_CONTINUOUS, ...) takes 3 arguments (%lu given)",
//                 (unsigned long)PyTuple_Size(o));
//             return NULL;
//         }
//         EMC_AXIS_JOG cont;
//         cont.axis = axis;
//         cont.vel = vel;
//         cont.serial_number = next_serial(s);
//         s->c->write(cont);
//         emcWaitCommandReceived(s->serial, s->s);
//     } else if(fn == LOCAL_JOG_INCREMENT) {
//         if(PyTuple_Size(o) != 4) {
//             PyErr_Format( PyExc_TypeError,
//                 "jog(JOG_INCREMENT, ...) takes 4 arguments (%lu given)",
//                 (unsigned long)PyTuple_Size(o));
//             return NULL;
//         }

//         EMC_AXIS_INCR_JOG incr;
//         incr.axis = axis;
//         incr.vel = vel;
//         incr.incr = inc;
//         incr.serial_number = next_serial(s);
//         s->c->write(incr);
//         emcWaitCommandReceived(s->serial, s->s);
//     } else {
//         PyErr_Format( PyExc_TypeError, "jog() first argument must be JOG_xxx");
//         return NULL;
//     }

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *reset_interpreter(pyCommandChannel *s, PyObject *o) {
//     EMC_TASK_PLAN_INIT m;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *program_open(pyCommandChannel *s, PyObject *o) {
//     EMC_TASK_PLAN_OPEN m;
//     char *file;
//     int len;

//     if(!PyArg_ParseTuple(o, "s#", &file, &len)) return NULL;
//     m.serial_number = next_serial(s);
//     strcpy(m.file, file);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *emcauto(pyCommandChannel *s, PyObject *o) {
//     int fn;
//     EMC_TASK_PLAN_RUN run;
//     EMC_TASK_PLAN_PAUSE pause;
//     EMC_TASK_PLAN_RESUME resume;
//     EMC_TASK_PLAN_STEP step;

//     if(PyArg_ParseTuple(o, "ii", &fn, &run.line) && fn == LOCAL_AUTO_RUN) {
//         run.serial_number = next_serial(s);
//         s->c->write(run);
//         emcWaitCommandReceived(s->serial, s->s);
//     } else {
//         PyErr_Clear();
//         if(!PyArg_ParseTuple(o, "i", &fn)) return NULL;
//         switch(fn) {
//         case LOCAL_AUTO_PAUSE:
//             pause.serial_number = next_serial(s);
//             s->c->write(pause);
//             emcWaitCommandReceived(s->serial, s->s);
//             break;
//         case LOCAL_AUTO_RESUME:
//             resume.serial_number = next_serial(s);
//             s->c->write(resume);
//             emcWaitCommandReceived(s->serial, s->s);
//             break;
//         case LOCAL_AUTO_STEP:
//             step.serial_number = next_serial(s);
//             s->c->write(step);
//             emcWaitCommandReceived(s->serial, s->s);
//             break;
//         default:
//             PyErr_Format(error, "Unexpected argument '%d' to command.auto", fn);
//             return NULL;
//         }
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *debug(pyCommandChannel *s, PyObject *o) {
//     EMC_SET_DEBUG d;

//     if(!PyArg_ParseTuple(o, "i", &d.debug)) return NULL;
//     d.serial_number = next_serial(s);
//     s->c->write(d);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *teleop(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_TELEOP_ENABLE en;

//     if(!PyArg_ParseTuple(o, "i", &en.enable)) return NULL;

//     en.serial_number = next_serial(s);
//     s->c->write(en);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_traj_mode(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_MODE mo;

//     if(!PyArg_ParseTuple(o, "i", &mo.mode)) return NULL;

//     mo.serial_number = next_serial(s);
//     s->c->write(mo);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_teleop_vector(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_TELEOP_VECTOR mo;

//     mo.vector.a = mo.vector.b = mo.vector.c = 0.;

//     if(!PyArg_ParseTuple(o, "ddd|ddd", &mo.vector.tran.x, &mo.vector.tran.y, &mo.vector.tran.z, &mo.vector.a, &mo.vector.b, &mo.vector.c))
//         return NULL;

//     mo.serial_number = next_serial(s);
//     s->c->write(mo);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_min_limit(pyCommandChannel *s, PyObject *o) {
//     EMC_AXIS_SET_MIN_POSITION_LIMIT m;
//     if(!PyArg_ParseTuple(o, "id", &m.axis, &m.limit))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_max_limit(pyCommandChannel *s, PyObject *o) {
//     EMC_AXIS_SET_MAX_POSITION_LIMIT m;
//     if(!PyArg_ParseTuple(o, "id", &m.axis, &m.limit))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_feed_override(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_FO_ENABLE m;
//     if(!PyArg_ParseTuple(o, "i", &m.mode))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_spindle_override(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_SO_ENABLE m;
//     if(!PyArg_ParseTuple(o, "i", &m.mode))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_feed_hold(pyCommandChannel *s, PyObject *o) {
//     EMC_TRAJ_SET_FH_ENABLE m;
//     if(!PyArg_ParseTuple(o, "i", &m.mode))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_adaptive_feed(pyCommandChannel *s, PyObject *o) {
//     EMC_MOTION_ADAPTIVE m;
//     if(!PyArg_ParseTuple(o, "i", &m.status))
//         return NULL;

//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_digital_output(pyCommandChannel *s, PyObject *o) {
//     EMC_MOTION_SET_DOUT m;
//     if(!PyArg_ParseTuple(o, "ii", &m.index, &m.start))
//         return NULL;

//     m.now = 1;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *set_analog_output(pyCommandChannel *s, PyObject *o) {
//     EMC_MOTION_SET_AOUT m;
//     if(!PyArg_ParseTuple(o, "id", &m.index, &m.start))
//         return NULL;

//     m.now = 1;
//     m.serial_number = next_serial(s);
//     s->c->write(m);
//     emcWaitCommandReceived(s->serial, s->s);

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *wait_complete(pyCommandChannel *s, PyObject *o) {
//     double timeout = EMC_COMMAND_TIMEOUT;
//     if (!PyArg_ParseTuple(o, "|d:emc.command.wait_complete", &timeout))
//         return NULL;
//     return PyInt_FromLong(emcWaitCommandComplete(s->serial, s->s, timeout));
// }

// static PyMemberDef Command_members[] = {
//     {(char*)"serial", T_INT, offsetof(pyCommandChannel, serial), READONLY},
//     {NULL}
// };

// static PyMethodDef Command_methods[] = {
//     {"debug", (PyCFunction)debug, METH_VARARGS},
//     {"teleop_enable", (PyCFunction)teleop, METH_VARARGS},
//     {"teleop_vector", (PyCFunction)set_teleop_vector, METH_VARARGS},
//     {"traj_mode", (PyCFunction)set_traj_mode, METH_VARARGS},
//     {"wait_complete", (PyCFunction)wait_complete, METH_VARARGS},
//     {"state", (PyCFunction)state, METH_VARARGS},
//     {"mdi", (PyCFunction)mdi, METH_VARARGS},
//     {"mode", (PyCFunction)mode, METH_VARARGS},
//     {"feedrate", (PyCFunction)feedrate, METH_VARARGS},
//     {"maxvel", (PyCFunction)maxvel, METH_VARARGS},
//     {"spindleoverride", (PyCFunction)spindleoverride, METH_VARARGS},
//     {"spindle", (PyCFunction)spindle, METH_VARARGS},
//     {"tool_offset", (PyCFunction)tool_offset, METH_VARARGS},
//     {"mist", (PyCFunction)mist, METH_VARARGS},
//     {"flood", (PyCFunction)flood, METH_VARARGS},
//     {"brake", (PyCFunction)brake, METH_VARARGS},
//     {"load_tool_table", (PyCFunction)load_tool_table, METH_NOARGS},
//     {"abort", (PyCFunction)emcabort, METH_NOARGS},
//     {"override_limits", (PyCFunction)override_limits, METH_NOARGS},
//     {"home", (PyCFunction)home, METH_VARARGS},
//     {"unhome", (PyCFunction)unhome, METH_VARARGS},
//     {"jog", (PyCFunction)jog, METH_VARARGS},
//     {"reset_interpreter", (PyCFunction)reset_interpreter, METH_NOARGS},
//     {"program_open", (PyCFunction)program_open, METH_VARARGS},
//     {"auto", (PyCFunction)emcauto, METH_VARARGS},
//     {"set_optional_stop", (PyCFunction)optional_stop, METH_VARARGS},
//     {"set_block_delete", (PyCFunction)block_delete, METH_VARARGS},
//     {"set_min_limit", (PyCFunction)set_min_limit, METH_VARARGS},
//     {"set_max_limit", (PyCFunction)set_max_limit, METH_VARARGS},
//     {"set_feed_override", (PyCFunction)set_feed_override, METH_VARARGS},
//     {"set_spindle_override", (PyCFunction)set_spindle_override, METH_VARARGS},
//     {"set_feed_hold", (PyCFunction)set_feed_hold, METH_VARARGS},
//     {"set_adaptive_feed", (PyCFunction)set_adaptive_feed, METH_VARARGS},
//     {"set_digital_output", (PyCFunction)set_digital_output, METH_VARARGS},
//     {"set_analog_output", (PyCFunction)set_analog_output, METH_VARARGS},
//     {NULL}
// };

// static PyTypeObject Command_Type = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "emc.command",          /*tp_name*/
//     sizeof(pyCommandChannel),/*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     (destructor)Command_dealloc,        /*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     Command_methods,        /*tp_methods*/
//     Command_members,        /*tp_members*/
//     0,                      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     (initproc)Command_init, /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// static int Error_init(pyErrorChannel *self, PyObject *a, PyObject *k) {
//     char *file = get_nmlfile();
//     if(file == NULL) return -1;

//     NML *c = new NML(emcFormat, "emcError", "xemc", file);
//     if(!c) {
//         PyErr_Format( error, "new NML failed");
//         return -1;
//     }

//     self->c = c;
//     return 0;
// }

// static PyObject* Error_poll(pyErrorChannel *s) {
//     if(!s->c->valid()) {
//         PyErr_Format( error, "Error buffer invalid" );
//         return NULL;
//     }
//     NMLTYPE type = s->c->read();
//     if(type == 0) {
//         Py_INCREF(Py_None);
//         return Py_None;
//     }
//     PyObject *r = PyTuple_New(2);
//     PyTuple_SET_ITEM(r, 0, PyInt_FromLong(type));
// #define PASTE(a,b) a ## b
// #define _TYPECASE(tag, type, f) \
//     case tag: { \
//         char error_string[LINELEN]; \
//         strncpy(error_string, ((type*)s->c->get_address())->f, LINELEN-1); \
//         error_string[LINELEN-1] = 0; \
//         PyTuple_SET_ITEM(r, 1, PyString_FromString(error_string)); \
//         break; \
//     }
// #define TYPECASE(x, f) _TYPECASE(PASTE(x, _TYPE), x, f)
//     switch(type) {
//         TYPECASE(EMC_OPERATOR_ERROR, error)
//         TYPECASE(EMC_OPERATOR_TEXT, text)
//         TYPECASE(EMC_OPERATOR_DISPLAY, display)
//         TYPECASE(NML_ERROR, error)
//         TYPECASE(NML_TEXT, text)
//         TYPECASE(NML_DISPLAY, display)
//     default:
//         {
//             char error_string[256];
//             sprintf(error_string, "unrecognized error %ld", type);
//             PyTuple_SET_ITEM(r, 1, PyString_FromString(error_string));
//             break;
//         }
//     }
//     return r;
// }

// static void Error_dealloc(PyObject *self) {
//     delete ((pyErrorChannel*)self)->c;
//     PyObject_Del(self);
// }

// static PyMethodDef Error_methods[] = {
//     {"poll", (PyCFunction)Error_poll, METH_NOARGS, "Poll for errors"},
//     {NULL}
// };

// static PyTypeObject Error_Type = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "emc.error_channel",    /*tp_name*/
//     sizeof(pyErrorChannel), /*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     (destructor)Error_dealloc,        /*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     Error_methods,          /*tp_methods*/
//     0,                      /*tp_members*/
//     0,                      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     (initproc)Error_init,   /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// #include <GL/gl.h>

// static void rotate_z(double pt[3], double a) {
//     double theta = a * M_PI / 180;
//     double c = cos(theta), s = sin(theta);
//     double tx, ty;
//     tx = pt[0] * c - pt[1] * s;
//     ty = pt[0] * s + pt[1] * c;

//     pt[0] = tx; pt[1] = ty;
// }

// static void rotate_y(double pt[3], double a) {
//     double theta = a * M_PI / 180;
//     double c = cos(theta), s = sin(theta);
//     double tx, tz;
//     tx = pt[0] * c - pt[2] * s;
//     tz = pt[0] * s + pt[2] * c;

//     pt[0] = tx; pt[2] = tz;
// }

// static void rotate_x(double pt[3], double a) {
//     double theta = a * M_PI / 180;
//     double c = cos(theta), s = sin(theta);
//     double tx, tz;
//     tx = pt[1] * c - pt[2] * s;
//     tz = pt[1] * s + pt[2] * c;

//     pt[1] = tx; pt[2] = tz;
// }

// static void translate(double pt[3], double ox, double oy, double oz) {
//     pt[0] += ox;
//     pt[1] += oy;
//     pt[2] += oz;
// }

// static void vertex9(const double pt[9], double p[3], const char *geometry) {
//     double sign = 1;

//     p[0] = 0;
//     p[1] = 0;
//     p[2] = 0;

//     for(; *geometry; geometry++) {
//         switch(*geometry) {
//             case '-': sign = -1; break;
//             case 'X': translate(p, pt[0] * sign, 0, 0); sign=1; break;
//             case 'Y': translate(p, 0, pt[1] * sign, 0); sign=1; break;
//             case 'Z': translate(p, 0, 0, pt[2] * sign); sign=1; break;
//             case 'U': translate(p, pt[6] * sign, 0, 0); sign=1; break;
//             case 'V': translate(p, 0, pt[7] * sign, 0); sign=1; break;
//             case 'W': translate(p, 0, 0, pt[8] * sign); sign=1; break;
//             case 'A': rotate_x(p, pt[3] * sign); sign=1; break;
//             case 'B': rotate_y(p, pt[4] * sign); sign=1; break;
//             case 'C': rotate_z(p, pt[5] * sign); sign=1; break;
//         }
//     }
// }

// static void glvertex9(const double pt[9], const char *geometry) {
//     double p[3];
//     vertex9(pt, p, geometry);
//     glVertex3dv(p);
// }

// #define max(a,b) ((a) < (b) ? (b) : (a))
// #define max3(a,b,c) (max((a),max((b),(c))))

// static void line9(const double p1[9], const double p2[9], const char *geometry) {
//     if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
//         double dc = max3(
//             fabs(p2[3] - p1[3]),
//             fabs(p2[4] - p1[4]),
//             fabs(p2[5] - p1[5]));
//         int st = (int)ceil(max(10, dc/10));
//         int i;

//         for(i=1; i<=st; i++) {
//             double t = i * 1.0 / st;
//             double v = 1.0 - t;
//             double pt[9];
//             for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
//             glvertex9(pt, geometry);
//         }
//     } else {
//         glvertex9(p2, geometry);
//     }
// }

// static void line9b(const double p1[9], const double p2[9], const char *geometry) {
//     glvertex9(p1, geometry);
//     if(p1[3] != p2[3] || p1[4] != p2[4] || p1[5] != p2[5]) {
//         double dc = max3(
//             fabs(p2[3] - p1[3]),
//             fabs(p2[4] - p1[4]),
//             fabs(p2[5] - p1[5]));
//         int st = (int)ceil(max(10, dc/10));
//         int i;

//         for(i=1; i<=st; i++) {
//             double t = i * 1.0 / st;
//             double v = 1.0 - t;
//             double pt[9];
//             for(int j=0; j<9; j++) { pt[j] = t * p2[j] + v * p1[j]; }
//             glvertex9(pt, geometry);
//             if(i != st)
//                 glvertex9(pt, geometry);
//         }
//     } else {
//         glvertex9(p2, geometry);
//     }
// }

// static PyObject *pyline9(PyObject *s, PyObject *o) {
//     double pt1[9], pt2[9];
//     const char *geometry;

//     if(!PyArg_ParseTuple(o, "s(ddddddddd)(ddddddddd):line9",
//             &geometry,
//             &pt1[0], &pt1[1], &pt1[2],
//             &pt1[3], &pt1[4], &pt1[5],
//             &pt1[6], &pt1[7], &pt1[8],
//             &pt2[0], &pt2[1], &pt2[2],
//             &pt2[3], &pt2[4], &pt2[5],
//             &pt2[6], &pt2[7], &pt2[8]))
//         return NULL;

//     line9b(pt1, pt2, geometry);

//     Py_RETURN_NONE;
// }

// static PyObject *pyvertex9(PyObject *s, PyObject *o) {
//     double pt1[9], pt[3];
//     char *geometry;
//     if(!PyArg_ParseTuple(o, "s(ddddddddd):vertex9",
//             &geometry,
//             &pt1[0], &pt1[1], &pt1[2],
//             &pt1[3], &pt1[4], &pt1[5],
//             &pt1[6], &pt1[7], &pt1[8]))
//         return NULL;

//     vertex9(pt, pt1, geometry);
//     return Py_BuildValue("(ddd)", &pt[0], &pt[1], &pt[2]);
// }

// static PyObject *pydraw_lines(PyObject *s, PyObject *o) {
//     PyListObject *li;
//     int for_selection = 0;
//     int i;
//     int first = 1;
//     int nl = -1, n;
//     double p1[9], p2[9], pl[9];
//     char *geometry;

//     if(!PyArg_ParseTuple(o, "sO!|i:draw_lines",
// 			    &geometry, &PyList_Type, &li, &for_selection))
//         return NULL;

//     for(i=0; i<PyList_GET_SIZE(li); i++) {
//         PyObject *it = PyList_GET_ITEM(li, i);
//         PyObject *dummy1, *dummy2, *dummy3;
//         if(!PyArg_ParseTuple(it, "i(ddddddddd)(ddddddddd)|OOO", &n,
//                     p1+0, p1+1, p1+2,
//                     p1+3, p1+4, p1+5,
//                     p1+6, p1+7, p1+8,
//                     p2+0, p2+1, p2+2,
//                     p2+3, p2+4, p2+5,
//                     p2+6, p2+7, p2+8,
//                     &dummy1, &dummy2, &dummy3)) {
//             if(!first) glEnd();
//             return NULL;
//         }
//         if(first || memcmp(p1, pl, sizeof(p1))
//                 || (for_selection && n != nl)) {
//             if(!first) glEnd();
//             if(for_selection && n != nl) {
//                 glLoadName(n);
//                 nl = n;
//             }
//             glBegin(GL_LINE_STRIP);
//             glvertex9(p1, geometry);
//             first = 0;
//         }
//         line9(p1, p2, geometry);
//         memcpy(pl, p2, sizeof(p1));
//     }

//     if(!first) glEnd();

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pydraw_dwells(PyObject *s, PyObject *o) {
//     PyListObject *li;
//     int for_selection = 0, is_lathe = 0, i, n;
//     double alpha;
//     char *geometry;
//     double delta = 0.015625;

//     if(!PyArg_ParseTuple(o, "sO!dii:draw_dwells", &geometry, &PyList_Type, &li, &alpha, &for_selection, &is_lathe))
//         return NULL;

//     if (for_selection == 0)
//         glBegin(GL_LINES);

//     for(i=0; i<PyList_GET_SIZE(li); i++) {
//         PyObject *it = PyList_GET_ITEM(li, i);
//         double red, green, blue, x, y, z;
//         int axis;
//         if(!PyArg_ParseTuple(it, "i(ddd)dddi", &n, &red, &green, &blue, &x, &y, &z, &axis)) {
//             return NULL;
//         }
//         if (for_selection != 1)
//             glColor4d(red, green, blue, alpha);
//         if (for_selection == 1) {
//             glLoadName(n);
//             glBegin(GL_LINES);
//         }
//         if (is_lathe == 1)
//             axis = 1;

//         if (axis == 0) {
//             glVertex3f(x-delta,y-delta,z);
//             glVertex3f(x+delta,y+delta,z);
//             glVertex3f(x-delta,y+delta,z);
//             glVertex3f(x+delta,y-delta,z);

//             glVertex3f(x+delta,y+delta,z);
//             glVertex3f(x-delta,y-delta,z);
//             glVertex3f(x+delta,y-delta,z);
//             glVertex3f(x-delta,y+delta,z);
//         } else if (axis == 1) {
//             glVertex3f(x-delta,y,z-delta);
//             glVertex3f(x+delta,y,z+delta);
//             glVertex3f(x-delta,y,z+delta);
//             glVertex3f(x+delta,y,z-delta);

//             glVertex3f(x+delta,y,z+delta);
//             glVertex3f(x-delta,y,z-delta);
//             glVertex3f(x+delta,y,z-delta);
//             glVertex3f(x-delta,y,z+delta);
//         } else {
//             glVertex3f(x,y-delta,z-delta);
//             glVertex3f(x,y+delta,z+delta);
//             glVertex3f(x,y+delta,z-delta);
//             glVertex3f(x,y-delta,z+delta);

//             glVertex3f(x,y+delta,z+delta);
//             glVertex3f(x,y-delta,z-delta);
//             glVertex3f(x,y-delta,z+delta);
//             glVertex3f(x,y+delta,z-delta);
//         }
//         if (for_selection == 1)
//             glEnd();
//     }

//     if (for_selection == 0)
//         glEnd();

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// struct color {
//     unsigned char r, g, b, a;
//     bool operator==(const color &o) const {
//         return r == o.r && g == o.g && b == o.b && a == o.a;
//     }
//     bool operator!=(const color &o) const {
//         return r != o.r || g != o.g || b != o.b || a != o.a;
//     }
// } color;

// struct logger_point {
//     float x, y, z;
//     float rx, ry, rz;
//     struct color c;
// };

// #define NUMCOLORS (6)
// #define MAX_POINTS (10000)
// typedef struct {
//     PyObject_HEAD
//     int npts, mpts, lpts;
//     struct logger_point *p;
//     struct color colors[NUMCOLORS];
//     bool exit, clear, changed;
//     char *geometry;
//     pyStatChannel *st;
// } pyPositionLogger;

// static const double epsilon = 1e-4; // 1-cos(1 deg) ~= 1e-4
// static const double tiny = 1e-10;

// static inline bool colinear(float xa, float ya, float za, float xb, float yb, float zb, float xc, float yc, float zc) {
//     double dx1 = xa-xb, dx2 = xb-xc;
//     double dy1 = ya-yb, dy2 = yb-yc;
//     double dz1 = za-zb, dz2 = zb-zc;
//     double dp = sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1);
//     double dq = sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
//     if( fabs(dp) < tiny || fabs(dq) < tiny ) return true;
//     double dot = (dx1*dx2 + dy1*dy2 + dz1*dz2) / dp / dq;
//     if( fabs(1-dot) < epsilon) return true;
//     return false;
// }

// static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// static void LOCK() { pthread_mutex_lock(&mutex); }
// static void UNLOCK() { pthread_mutex_unlock(&mutex); }

// static int Logger_init(pyPositionLogger *self, PyObject *a, PyObject *k) {
//     char *geometry;
//     struct color *c = self->colors;
//     self->p = (logger_point*)malloc(0);
//     self->npts = self->mpts = 0;
//     self->exit = self->clear = 0;
//     self->changed = 1;
//     self->st = 0;
//     if(!PyArg_ParseTuple(a, "O!(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)(BBBB)s",
//             &Stat_Type, &self->st,
//             &c[0].r,&c[0].g, &c[0].b, &c[0].a,
//             &c[1].r,&c[1].g, &c[1].b, &c[1].a,
//             &c[2].r,&c[2].g, &c[2].b, &c[2].a,
//             &c[3].r,&c[3].g, &c[3].b, &c[3].a,
//             &c[4].r,&c[4].g, &c[4].b, &c[4].a,
//             &c[5].r,&c[5].g, &c[5].b, &c[5].a,
//             &geometry
//             ))
//         return -1;
//     Py_INCREF(self->st);
//     self->geometry = strdup(geometry);
//     return 0;
// }

// static void Logger_dealloc(pyPositionLogger *s) {
//     free(s->p);
//     Py_XDECREF(s->st);
//     free(s->geometry);
//     PyObject_Del(s);
// }

// static PyObject *Logger_start(pyPositionLogger *s, PyObject *o) {
//     double interval;
//     struct timespec ts;

//     if(!PyArg_ParseTuple(o, "d:logger.start", &interval)) return NULL;
//     ts.tv_sec = (int)interval;
//     ts.tv_nsec = (long int)(1e9 * (interval - ts.tv_sec));

//     Py_INCREF(s->st);

//     s->exit = 0;
//     s->clear = 0;
//     s->npts = 0;

//     Py_BEGIN_ALLOW_THREADS
//     while(!s->exit) {
//         if(s->clear) {
//             s->npts = 0;
//             s->lpts = 0;
//             s->clear = 0;
//         }
//         if(s->st->c->valid() && s->st->c->peek() == EMC_STAT_TYPE) {
//             EMC_STAT *status = static_cast<EMC_STAT*>(s->st->c->get_address());
//             int colornum = 2;
//             colornum = status->motion.traj.motion_type;
//             if(colornum < 0 || colornum > NUMCOLORS) colornum = 0;

//             double pt[9] = {
//                 status->motion.traj.position.tran.x - status->task.toolOffset.tran.x,
//                 status->motion.traj.position.tran.y - status->task.toolOffset.tran.y,
//                 status->motion.traj.position.tran.z - status->task.toolOffset.tran.z,
//                 status->motion.traj.position.a - status->task.toolOffset.a,
//                 status->motion.traj.position.b - status->task.toolOffset.b,
//                 status->motion.traj.position.c - status->task.toolOffset.c,
//                 status->motion.traj.position.u - status->task.toolOffset.u,
//                 status->motion.traj.position.v - status->task.toolOffset.v,
//                 status->motion.traj.position.w - status->task.toolOffset.w};

//             double p[3];
//             vertex9(pt, p, s->geometry);
//             double x = p[0], y = p[1], z = p[2];
//             double rx = pt[3], ry = -pt[4], rz = pt[5];

//             struct color c = s->colors[colornum];
//             struct logger_point *op = &s->p[s->npts-1];
//             struct logger_point *oop = &s->p[s->npts-2];
//             bool add_point = s->npts < 2 || c != op->c || !colinear(
//                         x, y, z, op->x, op->y, op->z, oop->x, oop->y, oop->z);
//             if(add_point) {
//                 // 1 or 2 points may be added, make room whenever
//                 // fewer than 2 are left
//                 bool changed_color = s->npts && c != op->c;
//                 if(s->npts+2 > s->mpts) {
//                     LOCK();
//                     if(s->mpts >= MAX_POINTS) {
//                         int adjust = MAX_POINTS / 10;
//                         if(adjust < 2) adjust = 2;
//                         s->npts -= adjust;
//                         memmove(s->p, s->p + adjust,
//                                 sizeof(struct logger_point) * s->npts);
//                     } else {
//                         s->mpts = 2 * s->mpts + 2;
//                         s->changed = 1;
//                         s->p = (struct logger_point*) realloc(s->p,
//                                     sizeof(struct logger_point) * s->mpts);
//                     }
//                     UNLOCK();
//                     op = &s->p[s->npts-1];
//                     oop = &s->p[s->npts-2];
//                 }
//                 if(changed_color) {
//                     {
//                     struct logger_point &np = s->p[s->npts];
//                     np.x = op->x; np.y = op->y; np.z = op->z;
//                     np.rx = rx; np.ry = ry; np.rz = rz;
//                     np.c = c;
//                     }
//                     {
//                     struct logger_point &np = s->p[s->npts+1];
//                     np.x = x; np.y = y; np.z = z;
//                     np.rx = rx; np.ry = ry; np.rz = rz;
//                     np.c = c;
//                     }
//                     s->npts += 2;
//                 } else {
//                     struct logger_point &np = s->p[s->npts];
//                     np.x = x; np.y = y; np.z = z;
//                     np.rx = rx; np.ry = ry; np.rz = rz;
//                     np.c = c;
//                     s->npts++;
//                 }
//             } else {
//                 struct logger_point &np = s->p[s->npts-1];
//                 np.x = x; np.y = y; np.z = z;
//                 np.rx = rx; np.ry = ry; np.rz = rz;
//             }
//         }
//         nanosleep(&ts, NULL);
//     }
//     Py_END_ALLOW_THREADS
//     Py_DECREF(s->st);
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject* Logger_clear(pyPositionLogger *s, PyObject *o) {
//     s->clear = true;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject* Logger_stop(pyPositionLogger *s, PyObject *o) {
//     s->exit = true;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject* Logger_call(pyPositionLogger *s, PyObject *o) {
//     if(!s->clear) {
//         LOCK();
//         if(s->changed) {
//             glVertexPointer(3, GL_FLOAT,
//                     sizeof(struct logger_point), &s->p->x);
//             glColorPointer(4, GL_UNSIGNED_BYTE,
//                     sizeof(struct logger_point), &s->p->c);
//             glEnableClientState(GL_COLOR_ARRAY);
//             glEnableClientState(GL_VERTEX_ARRAY);
//             s->changed = 0;
//         }
//         s->lpts = s->npts;
//         glDrawArrays(GL_LINE_STRIP, 0, s->npts);
//         UNLOCK();
//     }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *Logger_last(pyPositionLogger *s, PyObject *o) {
//     int flag=1;
//     if(!PyArg_ParseTuple(o, "|i:emc.positionlogger.last", &flag)) return NULL;
//     PyObject *result = NULL;
//     LOCK();
//     int idx = flag ? s->lpts : s->npts;
//     if(!idx) {
//         Py_INCREF(Py_None);
//         result = Py_None;
//     } else {
//         result = PyTuple_New(6);
//         struct logger_point &p = s->p[idx-1];
//         PyTuple_SET_ITEM(result, 0, PyFloat_FromDouble(p.x));
//         PyTuple_SET_ITEM(result, 1, PyFloat_FromDouble(p.y));
//         PyTuple_SET_ITEM(result, 2, PyFloat_FromDouble(p.z));
//         PyTuple_SET_ITEM(result, 3, PyFloat_FromDouble(p.rx));
//         PyTuple_SET_ITEM(result, 4, PyFloat_FromDouble(p.ry));
//         PyTuple_SET_ITEM(result, 5, PyFloat_FromDouble(p.rz));
//     }
//     UNLOCK();
//     return result;
// }

// static PyMemberDef Logger_members[] = {
//     {(char*)"npts", T_INT, offsetof(pyPositionLogger, npts), READONLY},
//     {0, 0, 0, 0},
// };

// static PyMethodDef Logger_methods[] = {
//     {"start", (PyCFunction)Logger_start, METH_VARARGS,
//         "Start the position logger and run every ARG seconds"},
//     {"clear", (PyCFunction)Logger_clear, METH_NOARGS,
//         "Clear the position logger"},
//     {"stop", (PyCFunction)Logger_stop, METH_NOARGS,
//         "Stop the position logger"},
//     {"call", (PyCFunction)Logger_call, METH_NOARGS,
//         "Plot the backplot now"},
//     {"last", (PyCFunction)Logger_last, METH_VARARGS,
//         "Return the most recent point on the plot or None"},
//     {NULL, NULL, 0, NULL},
// };

// static PyTypeObject PositionLoggerType = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /*ob_size*/
//     "emc.positionlogger",   /*tp_name*/
//     sizeof(pyPositionLogger), /*tp_basicsize*/
//     0,                      /*tp_itemsize*/
//     /* methods */
//     (destructor)Logger_dealloc, /*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     Logger_methods,         /*tp_methods*/
//     Logger_members,         /*tp_members*/
//     0,                      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     (initproc)Logger_init,  /*tp_init*/
//     0,                      /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// static PyMethodDef emc_methods[] = {
// #define METH(name, doc) { #name, (PyCFunction) py##name, METH_VARARGS, doc }
// METH(draw_lines, "Draw a bunch of lines in the 'rs274.glcanon' format"),
// METH(draw_dwells, "Draw a bunch of dwell positions in the 'rs274.glcanon' format"),
// METH(line9, "Draw a single line in the 'rs274.glcanon' format; assumes glBegin(GL_LINES)"),
// METH(vertex9, "Get the 3d location for a 9d point"),
//     {NULL}
// #undef METH
// };

// /* ENUM defines an integer constant with the same name as the C constant.
//  * ENUMX defines an integer constant with the first i characters of the C
//  * constant name removed (so that ENUMX(4,RCS_TASK_MODE_MDI) creates a constant
//  * named TASK_MODE_MDI) */

// #define ENUM(e) PyModule_AddIntConstant(m, const_cast<char*>(#e), e)
// #define ENUMX(x,e) PyModule_AddIntConstant(m, x + const_cast<char*>(#e), e)

// PyMODINIT_FUNC
// initemc(void) {
//     emcInitGlobals();
//     verbose_nml_error_messages = 0;
//     clear_rcs_print_flag(~0);

//     m = Py_InitModule3("emc", emc_methods, "Interface to EMC");

//     PyType_Ready(&Stat_Type);
//     PyType_Ready(&Command_Type);
//     PyType_Ready(&Error_Type);
//     PyType_Ready(&Ini_Type);
//     error = PyErr_NewException((char*)"emc.error", PyExc_RuntimeError, NULL);

//     PyModule_AddObject(m, "stat", (PyObject*)&Stat_Type);
//     PyModule_AddObject(m, "command", (PyObject*)&Command_Type);
//     PyModule_AddObject(m, "error_channel", (PyObject*)&Error_Type);
//     PyModule_AddObject(m, "ini", (PyObject*)&Ini_Type);
//     PyModule_AddObject(m, "error", error);

//     PyType_Ready(&PositionLoggerType);
//     PyModule_AddObject(m, "positionlogger", (PyObject*)&PositionLoggerType);
//     pthread_mutex_init(&mutex, NULL);

//     PyModule_AddStringConstant(m, "nmlfile", EMC2_DEFAULT_NMLFILE);

//     PyModule_AddIntConstant(m, "OPERATOR_ERROR", EMC_OPERATOR_ERROR_TYPE);
//     PyModule_AddIntConstant(m, "OPERATOR_TEXT", EMC_OPERATOR_TEXT_TYPE);
//     PyModule_AddIntConstant(m, "OPERATOR_DISPLAY", EMC_OPERATOR_DISPLAY_TYPE);
//     PyModule_AddIntConstant(m, "NML_ERROR", NML_ERROR_TYPE);
//     PyModule_AddIntConstant(m, "NML_TEXT", NML_TEXT_TYPE);
//     PyModule_AddIntConstant(m, "NML_DISPLAY", NML_DISPLAY_TYPE);

//     PyStructSequence_InitType(&ToolResultType, &tool_result_desc);
//     PyModule_AddObject(m, "tool", (PyObject*)&ToolResultType);
//     PyModule_AddObject(m, "version", PyString_FromString(PACKAGE_VERSION));

//     ENUMX(4, EMC_AXIS_LINEAR);
//     ENUMX(4, EMC_AXIS_ANGULAR);

//     ENUMX(9, EMC_TASK_INTERP_IDLE);
//     ENUMX(9, EMC_TASK_INTERP_READING);
//     ENUMX(9, EMC_TASK_INTERP_PAUSED);
//     ENUMX(9, EMC_TASK_INTERP_WAITING);

//     ENUMX(9, EMC_TASK_MODE_MDI);
//     ENUMX(9, EMC_TASK_MODE_MANUAL);
//     ENUMX(9, EMC_TASK_MODE_AUTO);

//     ENUMX(9, EMC_TASK_STATE_OFF);
//     ENUMX(9, EMC_TASK_STATE_ON);
//     ENUMX(9, EMC_TASK_STATE_ESTOP);
//     ENUMX(9, EMC_TASK_STATE_ESTOP_RESET);

//     ENUMX(6, LOCAL_SPINDLE_FORWARD);
//     ENUMX(6, LOCAL_SPINDLE_REVERSE);
//     ENUMX(6, LOCAL_SPINDLE_OFF);
//     ENUMX(6, LOCAL_SPINDLE_INCREASE);
//     ENUMX(6, LOCAL_SPINDLE_DECREASE);
//     ENUMX(6, LOCAL_SPINDLE_CONSTANT);

//     ENUMX(6, LOCAL_MIST_ON);
//     ENUMX(6, LOCAL_MIST_OFF);

//     ENUMX(6, LOCAL_FLOOD_ON);
//     ENUMX(6, LOCAL_FLOOD_OFF);

//     ENUMX(6, LOCAL_BRAKE_ENGAGE);
//     ENUMX(6, LOCAL_BRAKE_RELEASE);

//     ENUMX(6, LOCAL_JOG_STOP);
//     ENUMX(6, LOCAL_JOG_CONTINUOUS);
//     ENUMX(6, LOCAL_JOG_INCREMENT);

//     ENUMX(6, LOCAL_AUTO_RUN);
//     ENUMX(6, LOCAL_AUTO_PAUSE);
//     ENUMX(6, LOCAL_AUTO_RESUME);
//     ENUMX(6, LOCAL_AUTO_STEP);

//     ENUMX(4, EMC_TRAJ_MODE_FREE);
//     ENUMX(4, EMC_TRAJ_MODE_COORD);
//     ENUMX(4, EMC_TRAJ_MODE_TELEOP);

//     ENUM(KINEMATICS_IDENTITY);
//     ENUM(KINEMATICS_FORWARD_ONLY);
//     ENUM(KINEMATICS_INVERSE_ONLY);
//     ENUM(KINEMATICS_BOTH);

//     ENUMX(4, EMC_DEBUG_CONFIG);
//     ENUMX(4, EMC_DEBUG_VERSIONS);
//     ENUMX(4, EMC_DEBUG_TASK_ISSUE);
//     ENUMX(4, EMC_DEBUG_NML);
//     ENUMX(4, EMC_DEBUG_MOTION_TIME);
//     ENUMX(4, EMC_DEBUG_INTERP);
//     ENUMX(4, EMC_DEBUG_RCS);
//     ENUMX(4, EMC_DEBUG_INTERP_LIST);

//     ENUMX(9, EMC_TASK_EXEC_ERROR);
//     ENUMX(9, EMC_TASK_EXEC_DONE);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION_QUEUE);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_IO);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_PAUSE);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_MOTION_AND_IO);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_DELAY);
//     ENUMX(9, EMC_TASK_EXEC_WAITING_FOR_SYSTEM_CMD);

//     ENUM(RCS_DONE);
//     ENUM(RCS_EXEC);
//     ENUM(RCS_ERROR);
// }


// // # vim:sw=4:sts=4:et:ts=8:

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

// #include <Python.h>
// #define GL_GLEXT_PROTOTYPES
// #include <GL/gl.h>
// #include <GL/glu.h>

// static PyObject *glerror;

// #define GLCALL0V(name) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     if(!PyArg_ParseTuple(o, ":" #name)) return NULL; \
//     name(); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }
// #define GLCALL1V(name, fmt, t1) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1)) return NULL; \
//     name(p1); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }
// #define GLCALL2V(name, fmt, t1, t2) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2)) return NULL; \
//     name(p1, p2); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }
// #define GLCALL3V(name, fmt, t1, t2, t3) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2, &p3)) return NULL; \
//     name(p1, p2, p3); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }

// #define GLCALL4V(name, fmt, t1, t2, t3, t4) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; t4 p4; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2, &p3, &p4)) return NULL; \
//     name(p1, p2, p3, p4); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }

// #define GLCALL5V(name, fmt, t1, t2, t3, t4, t5) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; t4 p4; t5 p5; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2, &p3, &p4, &p5)) \
// 	return NULL; \
//     name(p1, p2, p3, p4, p5); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }

// #define GLCALL6V(name, fmt, t1, t2, t3, t4, t5, t6) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; t4 p4; t5 p5; t6 p6; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2, &p3, &p4, &p5, &p6)) \
// 	return NULL; \
//     name(p1, p2, p3, p4, p5, p6); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }


// #define GLCALL7V(name, fmt, t1, t2, t3, t4, t5, t6, t7) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; t4 p4; t5 p5; t6 p6; t7 p7; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, &p1, &p2, &p3, &p4, &p5, &p6, &p7)) \
// 	return NULL; \
//     name(p1, p2, p3, p4, p5, p6, p7); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }

// #define GLCALL8V(name, fmt, t1, t2, t3, t4, t5, t6, t7, t8) \
// static PyObject *py##name(PyObject *s, PyObject *o) { \
//     t1 p1; t2 p2; t3 p3; t4 p4; t5 p5; t6 p6; t7 p7; t8 p8; \
//     if(!PyArg_ParseTuple(o, fmt ":" #name, \
// 		&p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8)) \
// 	return NULL; \
//     name(p1, p2, p3, p4, p5, p6, p7, p8); \
//     CHECK_ERROR; \
//     Py_INCREF(Py_None); return Py_None; \
// }

// #define CHECK_ERROR (void)0

// GLCALL1V(glBegin, "i", int)
// GLCALL3V(glColor3f, "fff", float, float, float)
// GLCALL4V(glColor4f, "ffff", float, float, float, float)
// GLCALL4V(glBlendColor, "ffff", float, float, float, float)
// GLCALL3V(glVertex3f, "fff", float, float, float);
// GLCALL2V(glLineStipple, "ii", int, int)
// GLCALL1V(glLineWidth, "f", float)

// GLCALL1V(glCallList, "i", int)
// GLCALL1V(glClear, "i", int)
// GLCALL4V(glClearColor, "ffff", float, float, float, float)
// GLCALL4V(glColorMask, "iiii", int, int, int, int);
// GLCALL1V(glDepthFunc, "i", int)
// GLCALL1V(glDepthMask, "i", int)
// GLCALL1V(glDisable, "i", int)
// GLCALL1V(glEnable, "i", int)
// GLCALL0V(glEndList)
// GLCALL1V(glFrontFace, "i", int)
// GLCALL0V(glInitNames)
// GLCALL3V(glLightf, "iif", int, int, float)
// GLCALL0V(glLoadIdentity)
// GLCALL1V(glLoadName, "i", int)
// GLCALL2V(glNewList, "ii", int, int)
// GLCALL3V(glNormal3f, "fff", float, float, float)
// GLCALL2V(glPixelZoom, "ff", float, float)
// GLCALL2V(glPolygonOffset, "ff", float, float)
// GLCALL0V(glPopMatrix)
// GLCALL0V(glPushMatrix)
// GLCALL1V(glPushAttrib, "i", int)
// GLCALL1V(glPushClientAttrib, "i", int)
// GLCALL0V(glPopAttrib)
// GLCALL0V(glPopClientAttrib)
// GLCALL1V(glPushName, "i", int)
// GLCALL2V(glRasterPos2i, "ii", int, int)
// GLCALL4V(glRectf, "ffff", float, float, float, float)
// GLCALL4V(glRotatef, "ffff", float, float, float, float)
// GLCALL3V(glScalef, "fff", float, float, float)
// GLCALL3V(glStencilFunc, "iii", int, int, int);
// GLCALL3V(glStencilOp, "iii", int, int, int);
// GLCALL1V(glDrawBuffer, "i", int)
// GLCALL3V(glDrawArrays, "iii", int, int, int)
// GLCALL1V(glMatrixMode, "i", int)
// GLCALL6V(glOrtho, "ffffff", float, float, float, float, float, float);
// GLCALL3V(glTranslatef, "fff", float, float, float);
// GLCALL4V(glViewport, "iiii", int, int, int, int);
// GLCALL4V(gluPerspective, "dddd", double, double, double, double);

// static void make_glerror(int code) {
//     PyObject *e = \
//         PyObject_CallFunction(glerror, "is", code, gluErrorString(code));
//     PyErr_SetObject(glerror, e);
// }

// #undef CHECK_ERROR
// #define CHECK_ERROR do { \
//     GLenum e = glGetError(); \
//     if(e) { make_glerror(e); return NULL; } \
// } while(0)

// GLCALL0V(glEnd)
// GLCALL2V(glDeleteLists, "ii", int, int)
// GLCALL2V(glBlendFunc, "ii", int, int)
// GLCALL0V(glFlush)
// GLCALL2V(glPixelStorei, "ii", int, int)

// static PyObject *pyglBitmap(PyObject *s, PyObject *o) {
//     int width, height, nbitmap;
//     float xorg, yorg, xmove, ymove;
//     char *bitmap;
//     if(!PyArg_ParseTuple(o, "iiffffs#:glBitmap", &width, &height,
//                 &xorg, &yorg, &xmove, &ymove, &bitmap, &nbitmap)) {
//         return NULL;
//     }
//     glBitmap(width, height, xorg, yorg, xmove, ymove, (GLubyte*)bitmap);
//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglDrawPixels(PyObject *s, PyObject *o) {
//     int width, height, format, type, nbitmap;
//     char *bitmap;
//     if(!PyArg_ParseTuple(o, "iiiis#:glBitmap", &width, &height,
//                 &format, &type, &bitmap, &nbitmap)) {
//         return NULL;
//     }
//     glDrawPixels(width, height, format, type, (GLubyte*)bitmap);
//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglGenLists(PyObject *s, PyObject *o) {
//     int range;
//     if(!PyArg_ParseTuple(o, "i:glGenLists", &range)) return NULL;
//     return PyInt_FromLong(glGenLists(range));
// }

// static PyObject *pyglGetDoublev(PyObject *s, PyObject *o) {
//     int what;
//     if(!PyArg_ParseTuple(o, "i:glGetDoublev", &what)) return NULL;
//     switch(what) {
//         case GL_MODELVIEW_MATRIX:
//         case GL_PROJECTION_MATRIX: {
//             double d[16];
//             PyObject *r = PyList_New(16);
//             int i;
//             glGetDoublev(what, d);
//             for(i=0; i<16; i++) {
//                 PyList_SetItem(r, i, PyFloat_FromDouble(d[i]));
//             }
//             return r;
//         }
//         default:
//             PyErr_Format(PyExc_ValueError,
// 			    "glGetDoublev does not support getting %d", what);
//             return NULL;
//     }
// }

// static PyObject *pyglGetIntegerv(PyObject *s, PyObject *o) {
//     int what;
//     if(!PyArg_ParseTuple(o, "i:glGetIntegerv", &what)) return NULL;
//     switch(what) {
//         case GL_LIST_INDEX: {
//             int r;
//             glGetIntegerv(what, &r);
//             return PyInt_FromLong(r);
//         }
//         case GL_VIEWPORT: {
//             int d[4];
//             PyObject *r = PyList_New(4);
//             int i;
//             glGetIntegerv(what, d);
//             for(i=0; i<4; i++) {
//                 PyList_SetItem(r, i, PyInt_FromLong(d[i]));
//             }
//             return r;
//         }
//         default:
//             PyErr_Format(PyExc_ValueError,
// 			    "glGetIntegerv does not support getting %d", what);
//             return NULL;
//     }
// }

// static PyObject *pyglInterleavedArrays(PyObject *s, PyObject *o) {
//     static void *buf = NULL;
//     PyObject *str;
//     int format, stride, size;

//     if(!PyArg_ParseTuple(o, "iiO:glInterleavedArrays", &format, &stride, &str))
//     {
//         return NULL;
//     }

//     if(!PyString_Check(str)) {
//         PyErr_Format( PyExc_TypeError, "Expected string" );
//         return NULL;
//     }

//     // size = min(8192, PyString_GET_SIZE(str));
//     size = PyString_GET_SIZE(str);
//     if(buf == NULL) buf = malloc(size);
//     else buf = realloc(buf, size);
//     memcpy(buf, PyString_AS_STRING(str), size);
//     glInterleavedArrays(format, stride, buf);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglLightModeli(PyObject *s, PyObject *o) {
//     int pname, param;
//     if(!PyArg_ParseTuple(o, "ii:glLightModeli", &pname, &param))
//         return NULL;
//     glLightModeli(pname, param);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglLightModelfv(PyObject *s, PyObject *o) {
//     int pname;
//     float param[4];
//     if(!PyArg_ParseTuple(o, "i(ffff):glLightModelfv",
// 			    &pname, param, param+1, param+2, param+3))
//         return NULL;
//     glLightModelfv(pname, param);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglLightfv(PyObject *s, PyObject *o) {
//     int light, pname;
//     float param[4];
//     if(!PyArg_ParseTuple(o, "ii(ffff):glLightfv",
// 			    &light, &pname, param, param+1, param+2, param+3))
//         return NULL;
//     glLightfv(light, pname, param);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglMaterialfv(PyObject *s, PyObject *o) {
//     int face, pname;
//     float param[4];
//     if(!PyArg_ParseTuple(o, "ii(ffff):glMaterialfv",
// 			    &face, &pname, param, param+1, param+2, param+3))
//         return NULL;
//     glMaterialfv(face, pname, param);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglMultMatrixd(PyObject *s, PyObject *o) {
//     double matrix[16];
//     if(!PyArg_ParseTuple(o, "(dddddddddddddddd):glMultMatrixd",
//             matrix, matrix+1, matrix+2, matrix+3,
//             matrix+4, matrix+5, matrix+6, matrix+7,
//             matrix+8, matrix+9, matrix+10, matrix+11,
//             matrix+12, matrix+13, matrix+14, matrix+15)) return NULL;

//     glMultMatrixd(matrix);

//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglPolygonStipple(PyObject *s, PyObject *o) {
//     char *buf;
//     int sz;
//     if(!PyArg_ParseTuple(o, "s#:glPolygonStipple", &buf, &sz)) return NULL;
//     if(sz != 128) {
//         PyErr_SetString(PyExc_ValueError, "Buffer must be 128 bytes long");
//         return NULL;
//     }
//     glPolygonStipple((GLubyte*)buf);
//     CHECK_ERROR;
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglReadPixels(PyObject *s, PyObject *o) {
//     int x, y, width, height, format=GL_RGB, type=GL_UNSIGNED_BYTE;
//     int sz;
//     char *buf;
//     PyObject *res;
//     if(!PyArg_ParseTuple(o, "iiii|ii", &x, &y, &width, &height, &format, &type))
//             return NULL;
//     sz = width * height * 4;
//     buf = malloc(sz);
//     glReadPixels(x,y,width,height,format,type,buf);
//     res = PyString_FromStringAndSize(buf, sz);
//     free(buf);
//     return res;
// }

// typedef struct {
//     PyObject_HEAD
//     GLUquadric *q;
// } Quadric;
// static void Quadric_dealloc(Quadric *q);
// // static PyObject *Quadric_repr(Quadric *q);

// static PyTypeObject Quadric_Type = {
//     PyObject_HEAD_INIT(NULL)
//     0,                      /* ob_size */
//     "minigl.quadric",       /* ob_name */
//     sizeof(Quadric), /* ob_basicsize */
//     0,                      /* ob_itemsize */
//     /* methods */
//     (destructor)Quadric_dealloc,/*tp_dealloc*/
//     0,                      /*tp_print*/
//     0,                      /*tp_getattr*/
//     0,                      /*tp_setattr*/
//     0,                      /*tp_compare*/
//     0,                      /*tp_repr*/
//     0,                      /*tp_as_number*/
//     0,                      /*tp_as_sequence*/
//     0,                      /*tp_as_mapping*/
//     0,                      /*tp_hash*/
//     0,                      /*tp_call*/
//     0,                      /*tp_str*/
//     0,                      /*tp_getattro*/
//     0,                      /*tp_setattro*/
//     0,                      /*tp_as_buffer*/
//     Py_TPFLAGS_DEFAULT,     /*tp_flags*/
//     0,                      /*tp_doc*/
//     0,                      /*tp_traverse*/
//     0,                      /*tp_clear*/
//     0,                      /*tp_richcompare*/
//     0,                      /*tp_weaklistoffset*/
//     0,                      /*tp_iter*/
//     0,                      /*tp_iternext*/
//     0,                      /*tp_methods*/
//     0,                      /*tp_members*/
//     0,                      /*tp_getset*/
//     0,                      /*tp_base*/
//     0,                      /*tp_dict*/
//     0,                      /*tp_descr_get*/
//     0,                      /*tp_descr_set*/
//     0,                      /*tp_dictoffset*/
//     0,                      /*tp_init*/
//     PyType_GenericAlloc,    /*tp_alloc*/
//     PyType_GenericNew,      /*tp_new*/
//     0,                      /*tp_free*/
//     0,                      /*tp_is_gc*/
// };

// static void Quadric_dealloc(Quadric *q) {
//     if(q->q) { gluDeleteQuadric(q->q); }
//     PyObject_Del(q);
// }

// static Quadric *pygluNewQuadric(PyObject *s, PyObject *o) {
//     Quadric *q = PyObject_New(Quadric, &Quadric_Type);
//     if(q) q->q = gluNewQuadric();
//     return q;
// }

// static PyObject *pygluDeleteQuadric(PyObject *s, PyObject *o) {
//     Quadric *q;
//     if(!PyArg_ParseTuple(o, "O!:gluDeleteQuadric", &Quadric_Type, &q))
// 	    return NULL;
//     if(q->q) { gluDeleteQuadric(q->q); q->q = NULL; }
//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluSphere(PyObject *s, PyObject *o) {
//     Quadric *q;
//     double radius;
//     int slices, stacks;

//     if(!PyArg_ParseTuple(o, "O!dii:gluSphere",
// 			    &Quadric_Type, &q, &radius,
// 			    &slices, &stacks))
//         return NULL;

//     if(!q->q) {
//         PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
//         return NULL;
//     }

//     gluSphere(q->q, radius, slices, stacks);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }


// static PyObject *pygluCylinder(PyObject *s, PyObject *o) {
//     Quadric *q;
//     double base, top, height;
//     int slices, stacks;

//     if(!PyArg_ParseTuple(o, "O!dddii:gluCylinder",
// 			    &Quadric_Type, &q, &base, &top,
// 			    &height, &slices, &stacks))
//         return NULL;

//     if(!q->q) {
//         PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
//         return NULL;
//     }

//     gluCylinder(q->q, base, top, height, slices, stacks);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluDisk(PyObject *s, PyObject *o) {
//     Quadric *q;
//     double inner, outer;
//     int slices, loops;

//     if(!PyArg_ParseTuple(o, "O!ddii:gluDisk",
// 			    &Quadric_Type, &q, &inner, &outer,
// 			    &slices, &loops))
//         return NULL;

//     if(!q->q) {
//         PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
//         return NULL;
//     }

//     gluDisk(q->q, inner, outer, slices, loops);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluQuadricOrientation(PyObject *s, PyObject *o) {
//     Quadric *q;
//     int orient;
//     if(!PyArg_ParseTuple(o, "O!i:gluQuadricOrientation",
// 			    &Quadric_Type, &q, &orient)) return NULL;
//     if(!q->q) {
//         PyErr_SetString(PyExc_TypeError, "Operation on deleted quadric");
//         return NULL;
//     }
//     gluQuadricOrientation(q->q, orient);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluLookAt(PyObject *s, PyObject *o) {
//     double eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz;
//     if(!PyArg_ParseTuple(o, "ddddddddd:gluLookAt", &eyex, &eyey, &eyez,
//                 &centerx, &centery, &centerz, &upx, &upy, &upz))
//         return NULL;
//     gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluPickMatrix(PyObject *s, PyObject *o) {
//     double x, y, delx, dely;
//     int viewport[4];
//     if(!PyArg_ParseTuple(o, "dddd(iiii):gluPickMatrix", &x, &y, &delx, &dely,
//                 viewport, viewport+1, viewport+2, viewport+3))
//         return NULL;
//     gluPickMatrix(x, y, delx, dely, viewport);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pygluProject(PyObject *s, PyObject *o) {
//     double x, y, z, wx, wy, wz, model[16], proj[16];
//     int viewport[4];

//     if(!PyArg_ParseTuple(o, "ddd:gluProject", &x, &y, &z))
//         return NULL;

//     glGetIntegerv(GL_VIEWPORT, viewport);
//     glGetDoublev(GL_MODELVIEW_MATRIX, model);
//     glGetDoublev(GL_PROJECTION_MATRIX, proj);

//     gluProject(x, y, z, model, proj, viewport, &wx, &wy, &wz);

//     CHECK_ERROR;

//     return Py_BuildValue("ddd", wx, wy, wz);
// }

// static PyObject *pygluUnProject(PyObject *s, PyObject *o) {
//     double x, y, z, wx, wy, wz, model[16], proj[16];
//     int viewport[4];

//     if(!PyArg_ParseTuple(o, "ddd:gluUnProject", &x, &y, &z))
//         return NULL;

//     glGetIntegerv(GL_VIEWPORT, viewport);
//     glGetDoublev(GL_MODELVIEW_MATRIX, model);
//     glGetDoublev(GL_PROJECTION_MATRIX, proj);

//     gluUnProject(x, y, z, model, proj, viewport, &wx, &wy, &wz);

//     CHECK_ERROR;

//     return Py_BuildValue("ddd", wx, wy, wz);
// }

// static GLuint *select_buffer = NULL;
// static PyObject *pyglSelectBuffer( PyObject *s, PyObject *o) {
//     int sz;
//     if(!PyArg_ParseTuple(o, "i:glSelectBuffer", &sz))
//         return NULL;
//     if(select_buffer) select_buffer = realloc( select_buffer, sizeof(int) * sz);
//     else select_buffer = malloc(sizeof(int) * sz);

//     glSelectBuffer(sz, select_buffer);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;

// }

// static GLfloat *feedback_buffer = NULL;
// static PyObject *pyglFeedbackBuffer( PyObject *s, PyObject *o) {
//     int sz, ty;
//     if(!PyArg_ParseTuple(o, "ii:glFeedbackBuffer", &sz, &ty))
//         return NULL;
//     if(feedback_buffer)
// 	    feedback_buffer = realloc( feedback_buffer, sizeof(int) * sz);
//     else feedback_buffer = malloc(sizeof(int) * sz);

//     glFeedbackBuffer(sz, ty, feedback_buffer);

//     CHECK_ERROR;

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// static PyObject *pyglRenderMode( PyObject *s, PyObject *o) {
//     int mode, lastmode, count;
//     if(!PyArg_ParseTuple(o, "i:glRenderMode", &mode))
//         return NULL;

//     glGetIntegerv(GL_RENDER_MODE, &lastmode);
//     count = glRenderMode(mode);

//     CHECK_ERROR;

//     if(count < 0) {
// 	PyErr_Format(PyExc_OverflowError, "Buffer too small");
// 	return 0;
//     }

//     if(lastmode == GL_SELECT) {
//         PyObject *r = PyList_New(0);
//         int i = 0;
//         while(i < count) {
//             PyObject *record = PyTuple_New(3);
//             int namelen = select_buffer[i++];
//             PyObject *name = PyList_New(namelen);
//             int j;
//             PyTuple_SetItem(record, 0,
//                     PyFloat_FromDouble(select_buffer[i++] / 214748364.));
//             PyTuple_SetItem(record, 1,
//                     PyFloat_FromDouble(select_buffer[i++] / 214748364.));
//             for(j=0; namelen; namelen--, j++, i++)
//                 PyList_SetItem(name, j, PyInt_FromLong(select_buffer[i]));
//             PyTuple_SetItem(record, 2, name);
//             PyList_Append(r, record);
//             Py_DECREF(record);
//         }
//         return r;
//     }
//     else if(lastmode == GL_FEEDBACK ) {
// 	PyObject *r = PyList_New(count);
// 	int i;
// 	for(i=0; i < count; i++) {
// 		PyList_SET_ITEM(r, i, PyFloat_FromDouble(feedback_buffer[i]));
// 	}
// 	return r;

//     }

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// /*
// glSelectBuffer
// */

// static PyMethodDef methods[] = {
// #define METH(name, doc) { #name, (PyCFunction) py##name, METH_VARARGS, doc }
// METH(glBegin,
//     "delimit the vertices of a primitive or a group of like primitives"),
// METH(glColor3f, "set the current color"),
// METH(glColor4f, "set the current color"),
// METH(glBlendColor, "set the blend color"),
// METH(glDeleteLists, "delete a contiguous group of display lists"),
// METH(glBlendFunc, "specify pixel arithmetic"),
// METH(glCallList, "execute a display list"),
// METH(glClear, "clear buffers to preset values"),
// METH(glClearColor, "specify clear values for the color buffers"),
// METH(glColorMask, "specify which components of color to write"),
// METH(glDepthFunc, "specify the value used for depth buffer comparisons"),
// METH(glDepthMask, "enable or disable writing into the depth buffer"),
// METH(glDisable, "enable or disable server-side GL capabilities"),
// METH(glEnable, "enable or disable server-side GL capabilities"),
// METH(glEnd,
//     "delimit the vertices of a primitive or a group of like primitives"),
// METH(glEndList, "create or replace a display list"),
// METH(glFlush, "force execution of GL commands in finite time"),
// METH(glFrontFace, "define front- and back-facing polygons"),
// METH(glInitNames, "initialize the name stack"),
// METH(glLightf, "set light source parameters"),
// METH(glLineStipple, "specify the line stipple pattern"),
// METH(glLineWidth, "specify the width of rasterized lines"),
// METH(glLoadIdentity, "replace the current matrix with the identity matrix"),
// METH(glLoadName, "load a name onto the name stack"),
// METH(glMatrixMode, "specify which matrix is the current matrix"),
// METH(glNewList, "create or replace a display list"),
// METH(glNormal3f, "set the current normal vector"),
// METH(glPixelZoom, "specify the pixel zoom factors"),
// METH(glPolygonOffset, "set the scale and units used to calculate depth values"),
// METH(glPolygonStipple, "set the polygon stippling pattern"),
// METH(glPopMatrix, "push and pop the current matrix stack"),
// METH(glPushMatrix, "push and pop the current matrix stack"),
// METH(glPopAttrib, "push and pop the current attribute stack"),
// METH(glPushAttrib, "push and pop the current attribute stack"),
// METH(glPopClientAttrib, "push and pop the current client attribute stack"),
// METH(glPushClientAttrib, "push and pop the current client attribute stack"),
// METH(glPushName, "push and pop the name stack"),
// METH(glRenderMode, "set rasterization mode"),
// METH(glRasterPos2i, "specify the raster position for pixel operations"),
// METH(glRectf, "draw a rectangle"),
// METH(glRotatef, "multiply the current matrix by a rotation matrix"),
// METH(glScalef, "multiply the current matrix by a general scaling matrix"),
// METH(glStencilFunc, "specify the stencil buffer test function"),
// METH(glStencilOp, "specify the stencil buffer operation"),
// METH(glFlush, "force execution of GL commands in finite time"),
// METH(glDrawBuffer, "specify which color buffers are to be drawn into"),
// METH(glDrawArrays, "render primitives from array data"),
// METH(glDrawPixels, "write a block of pixels to the frame buffer"),
// METH(glMatrixMode, "specify which matrix is the current matrix"),
// METH(glOrtho, "multiply the current matrix with an orthographic matrix"),
// METH(glTranslatef, "multiply the current matrix by a translation matrix"),
// METH(glVertex3f, "specify a vertex"),
// METH(glViewport, "set the viewport"),
// METH(gluPerspective, "set up a perspective projection matrix"),

// METH(glGenLists, "generate a contiguous set of empty display lists"),
// METH(glGetDoublev, "return the value or values of a selected parameter"),
// METH(glGetIntegerv, "return the value or values of a selected parameter"),
// METH(glInterleavedArrays,
//     "simultaneously specify and enable several interleaved arrays"),
// METH(glLightfv, "set light source parameters"),
// METH(glLightModelfv, "set the lighting model parameters"),
// METH(glLightModeli, "set the lighting model parameters"),
// METH(glMaterialfv, "specify material parameters for the lighting model"),
// METH(glMultMatrixd, "multiply the current matrix with the specified matrix"),

// METH(glPixelStorei, "set pixel storage modes"),

// METH(glSelectBuffer, "establish a buffer for selection mode values"),
// METH(glFeedbackBuffer, "establish a buffer for feedback mode values"),
// // METH(glVertex3fv, ""),
// METH(gluSphere, "draw a sphere"),
// METH(gluCylinder, "draw a cylinder"),
// METH(gluDeleteQuadric, "destroy a quadrics object"),
// METH(gluDisk, "draw a disk"),
// METH(gluLookAt, "define a viewing transformation"),
// METH(gluNewQuadric, "create a quadrics object"),
// METH(gluPickMatrix, "define a picking region"),
// METH(gluProject, "map object coordinates to window coordinates"),
// METH(gluQuadricOrientation, "specify inside/outside orientation for quadrics"),
// METH(gluUnProject, "map window coordinates to object coordinates"),
// METH(glBitmap, "draw a bitmap"),
// METH(glReadPixels, "read pixels"),


// #undef METH
// {NULL, NULL, 0, 0},
// };

// #define CONST(x) PyObject_SetAttrString(m, #x, PyInt_FromLong(x))
// void initminigl(void) {
//     PyObject *m = \
//     Py_InitModule3("minigl", methods, "Mini version of pyopengl for axis");
//     glerror = PyErr_NewException("minigl.error", PyExc_RuntimeError, NULL);
//     PyObject_SetAttrString(m, "error", glerror);
//     CONST(GL_ALWAYS);
//     CONST(GL_LEQUAL);
//     CONST(GL_BACK);
//     CONST(GL_BLEND);
//     CONST(GL_COLOR_BUFFER_BIT);
//     CONST(GL_COMPILE);
//     CONST(GL_CULL_FACE);
//     CONST(GL_DEPTH_BUFFER_BIT);
//     CONST(GL_DEPTH_TEST);
//     CONST(GL_FALSE);
//     CONST(GL_FRONT);
//     CONST(GL_FRONT_AND_BACK);
//     CONST(GL_KEEP);
//     CONST(GL_LESS);
//     CONST(GL_LIGHTING);
//     CONST(GL_LIGHTING_BIT);
//     CONST(GL_LIGHT_MODEL_AMBIENT);
//     CONST(GL_LIGHT_MODEL_LOCAL_VIEWER);
//     CONST(GL_LINES);
//     CONST(GL_LINE_LOOP);
//     CONST(GL_LINE_STIPPLE);
//     CONST(GL_LINE_STRIP);
//     CONST(GL_MODELVIEW);
//     CONST(GL_MODELVIEW_MATRIX);
//     CONST(GL_ONE_MINUS_SRC_ALPHA);
//     CONST(GL_CONSTANT_ALPHA);
//     CONST(GL_ONE_MINUS_CONSTANT_ALPHA);
//     CONST(GL_ONE);
//     CONST(GL_PROJECTION);
//     CONST(GL_PROJECTION_MATRIX);
//     CONST(GL_QUADS);
//     CONST(GL_QUAD_STRIP);
//     CONST(GL_RENDER);
//     CONST(GL_REPLACE);
//     CONST(GL_SELECT);
//     CONST(GL_FEEDBACK);
//     CONST(GL_SRC_ALPHA);
//     CONST(GL_STACK_OVERFLOW);
//     CONST(GL_STENCIL_BUFFER_BIT);
//     CONST(GL_STENCIL_TEST);
//     CONST(GL_TRUE);
//     CONST(GL_UNPACK_ALIGNMENT);
//     CONST(GL_V3F);
//     CONST(GL_C3F_V3F);
//     CONST(GL_C4UB_V3F);
//     CONST(GL_VIEWPORT);
//     CONST(GL_LIGHT0);
//     CONST(GL_POSITION);
//     CONST(GL_AMBIENT);
//     CONST(GL_AMBIENT_AND_DIFFUSE);
//     CONST(GL_DIFFUSE);
//     CONST(GL_CCW);
//     CONST(GL_DITHER);
//     CONST(GL_AUTO_NORMAL);
//     CONST(GL_NORMALIZE);
//     CONST(GL_POLYGON_OFFSET_FILL);
//     CONST(GL_POLYGON_STIPPLE);
//     CONST(GL_POLYGON);
//     CONST(GL_GREATER);
//     CONST(GL_LIST_INDEX);
//     CONST(GL_TRIANGLES);
//     CONST(GL_TRIANGLE_STRIP);
//     CONST(GL_TRIANGLE_FAN);
//     CONST(GLU_INSIDE);
//     CONST(GLU_OUTSIDE);
//     CONST(GL_TEXTURE_2D);
//     CONST(GL_2D);
//     CONST(GL_3D);
//     CONST(GL_3D_COLOR);
//     CONST(GL_3D_COLOR_TEXTURE);
//     CONST(GL_4D_COLOR_TEXTURE);
//     CONST(GL_COMPILE_AND_EXECUTE);
//     CONST(GL_CLIENT_PIXEL_STORE_BIT);
//     CONST(GL_UNPACK_SWAP_BYTES);
//     CONST(GL_UNPACK_LSB_FIRST);
//     CONST(GL_UNPACK_ROW_LENGTH);
//     CONST(GL_UNPACK_IMAGE_HEIGHT);
//     CONST(GL_UNPACK_SKIP_PIXELS);
//     CONST(GL_UNPACK_SKIP_ROWS);
//     CONST(GL_UNPACK_SKIP_IMAGES);
//     CONST(GL_UNPACK_ALIGNMENT);
//     CONST(GL_LUMINANCE);
//     CONST(GL_UNSIGNED_BYTE);

// }
// #include <Python.h>
// #include <emc/usr_intf/axis/extensions/togl.c>
// static int first_time = 1;

// static Tcl_Interp *get_interpreter(PyObject *tkapp) {
//     long interpaddr;
//     PyObject *interpaddrobj = PyObject_CallMethod(tkapp, "interpaddr", NULL);
//     if(interpaddrobj == NULL) { return NULL; }
//     interpaddr = PyInt_AsLong(interpaddrobj);
//     Py_DECREF(interpaddrobj);
//     if(interpaddr == -1) { return NULL; }
//     return (Tcl_Interp*)interpaddr;
// }

// PyObject *install(PyObject *s, PyObject *arg) {
//     Tcl_Interp *trp = get_interpreter(arg);
//     if(!trp) {
//         PyErr_SetString(PyExc_TypeError, "get_interpreter() returned NULL");
//         return NULL;
//     }
//     if (Tcl_InitStubs(trp, "8.1", 0) == NULL)
//     {
//         PyErr_SetString(PyExc_RuntimeError, "Tcl_InitStubs returned NULL");
//         return NULL;
//     }
//     if (Tk_InitStubs(trp, "8.1", 0) == NULL)
//     {
//         PyErr_SetString(PyExc_RuntimeError, "Tk_InitStubs returned NULL");
//         return NULL;
//     }
//     if (Tcl_PkgPresent(trp, "Togl", TOGL_VERSION, 0)) {
//         Py_INCREF(Py_None);
//         return Py_None;
//     }
//     if (Tcl_PkgProvide(trp, "Togl", TOGL_VERSION) != TCL_OK) {
//         PyErr_Format(PyExc_RuntimeError, "Tcl_PkgProvide failed: %s", trp->result);
//         return NULL;
//     }

//     Tcl_CreateCommand(trp, "togl", (Tcl_CmdProc *)Togl_Cmd,
//                       (ClientData) Tk_MainWindow(trp), NULL);

//     if(first_time) {
//         Tcl_InitHashTable(&CommandTable, TCL_STRING_KEYS);
//         first_time = 0;
//     }

//     Py_INCREF(Py_None);
//     return Py_None;
// }

// PyMethodDef togl_methods[] = {
//     {"install", (PyCFunction)install, METH_O, "install togl in a tkinter application"},
//     {NULL}
// };

// PyMODINIT_FUNC
// init_togl(void) {
//     Py_InitModule3("_togl", togl_methods, "togl extension for Tkinter");
// }
