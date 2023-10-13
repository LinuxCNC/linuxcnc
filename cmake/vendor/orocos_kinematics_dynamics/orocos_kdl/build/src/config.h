// Copyright  (C)  2014  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Brian Jensen <Jensen dot J dot Brian at gmail dot com>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef KDL_CONFIG_H
#define KDL_CONFIG_H

#define KDL_VERSION_MAJOR 1
#define KDL_VERSION_MINOR 4
#define KDL_VERSION_PATCH 0

#define KDL_VERSION (KDL_VERSION_MAJOR << 16) | (KDL_VERSION_MINOR << 8) | KDL_VERSION_PATCH

#define KDL_VERSION_STRING "1.4.0"

//Set which version of the Tree Interface to use
#define HAVE_STL_CONTAINER_INCOMPLETE_TYPES
/* #undef KDL_USE_NEW_TREE_INTERFACE */

#endif //#define KDL_CONFIG_H
