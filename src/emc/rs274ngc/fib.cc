/*
 * fib.c
 *
 *  Michael Haberler, 17.05.2010
 *
 *  $Revision: 1.1 $
 *
 *  see http://superjared.com/entry/anatomy-python-c-module/
 */

#include <Python.h>
#include <structmember.h>

#include "rs274ngc.hh"
#include "rs274ngc_interp.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "canon.hh"
#include "config.h"		// LINELEN




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
    {0,0,0,0}
};


PyMODINIT_FUNC
initfib(void) {
    fprintf(stderr,"initfib called\n");

       (void) Py_InitModule3("fib", methods, "demo");
        PyErr_Print();
	//	settings = get_setup();
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
