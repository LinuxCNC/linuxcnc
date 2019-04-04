#ifndef BOOST_PYENUM_MACROS_HH
#define BOOST_PYENUM_MACROS_HH
#include <boost/python/enum.hpp>

#define BOOST_PYENUM_VAL(X) value(#X, X)
#define BOOST_PYENUM_(X) enum_<X>(#X)

#endif // BOOST_PYENUM_MACROS_HH
