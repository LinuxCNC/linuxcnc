#ifndef BOOST_ATOMIC_DETAIL_CONFIG_HPP
#define BOOST_ATOMIC_DETAIL_CONFIG_HPP

//  Copyright (c) 2012 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#ifndef BOOST_DELETED_FUNCTION
#   define BOOST_DELETED_FUNCTION(fun) private: fun;
#endif


#ifndef BOOST_DEFAULTED_FUNCTION
#   define BOOST_DEFAULTED_FUNCTION(fun, body) fun body
#endif

#ifndef BOOST_NOEXCEPT
#   define BOOST_NOEXCEPT
#endif

#endif
