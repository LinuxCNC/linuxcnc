/*    This is a component of LinuxCNC
 *    Copyright 2013 Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef BOOST_PYTHON_NAX_ARITY
#define BOOST_PYTHON_MAX_ARITY 4
#endif
#include <boost/python/list.hpp>

struct ParamClass {

    Interp &interp;

    ParamClass(Interp &i);
    double getitem( boost::python::object sub);
    double setitem(boost::python::object sub, double dvalue);
    boost::python::list namelist(context &c) const;
    boost::python::list locals();
    boost::python::list globals();
    boost::python::list operator()() const;
    int length();
};

extern void export_ParamClass();
