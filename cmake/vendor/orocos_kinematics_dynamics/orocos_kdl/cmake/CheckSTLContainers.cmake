# - Macro to check whether the STL containers support incomplete types
# The issue at stake is discussed here in depth:
# http://www.drdobbs.com/the-standard-librarian-containers-of-inc/184403814
#
# Empirically libstdc++ and MSVC++ support containers of incomplete types
# whereas libc++ does not.
#
# The result is returned in HAVE_STL_CONTAINER_INCOMPLETE_TYPES
#
# Copyright 2014 Brian Jensen <Jensen dot J dot Brian at gmail dot com>
# Author: Brian Jensen <Jensen dot J dot Brian at gmail dot com>
#

macro(CHECK_STL_CONTAINERS)
    INCLUDE(CheckCXXSourceCompiles)
    SET(CMAKE_REQUIRED_FLAGS)
    CHECK_CXX_SOURCE_COMPILES("
        #include <string>
        #include <map>
        #include <vector>

        class TreeElement;
        typedef std::map<std::string, TreeElement> SegmentMap;

        class TreeElement
        {
            TreeElement(const std::string& name): number(0) {}

        public:
            int number;
            SegmentMap::const_iterator parent;
            std::vector<SegmentMap::const_iterator> children;

            static TreeElement Root(std::string& name)
            {
                return TreeElement(name);
            }
        };

        int main()
        {
            return 0;
        }
        "
        HAVE_STL_CONTAINER_INCOMPLETE_TYPES)

endmacro(CHECK_STL_CONTAINERS)
