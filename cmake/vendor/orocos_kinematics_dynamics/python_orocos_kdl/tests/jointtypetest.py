# Copyright  (C)  2020  Ruben Smits <ruben dot smits at intermodalics dot eu>

# Version: 1.0
# Author: Matthijs van der Burgh <MatthijsBurgh at outlook dot com>
# Maintainer: Ruben Smits <ruben dot smits at intermodalics dot eu>
# URL: http://www.orocos.org/kdl

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


from PyKDL import Joint
import unittest


class JointTypeNoneTest(unittest.TestCase):
    def testJointType(self):
        self.assertEqual(Joint.Fixed, Joint.None)
        self.assertEqual(str(Joint.Fixed), str(Joint.None))
        self.assertEqual(int(Joint.Fixed), int(Joint.None))


def suite():
    suite = unittest.TestSuite()
    suite.addTest(JointTypeNoneTest('testJointType'))
    return suite


if __name__ == '__main__':
    import sys
    suite = suite()
    result = unittest.TextTestRunner(verbosity=3).run(suite)

    if result.wasSuccessful():
        sys.exit(0)
    else:
        sys.exit(1)
