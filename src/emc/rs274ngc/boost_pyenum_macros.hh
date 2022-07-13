/**
 * @file boost_pyenum_macros.hh
 *
 * Simple wrapper macros that avoid name repetition when defining python enums with boost::python.
 *
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * @copyright Copyright 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#ifndef BOOST_PYENUM_MACROS_HH
#define BOOST_PYENUM_MACROS_HH
#include <boost/python/enum.hpp>

#define BOOST_PYENUM_VAL(X) value(#X, X)
#define BOOST_PYENUM_(X) enum_<X>(#X)

#endif // BOOST_PYENUM_MACROS_HH
