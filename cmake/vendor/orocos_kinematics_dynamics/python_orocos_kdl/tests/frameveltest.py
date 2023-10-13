# Copyright  (C)  2020  Ruben Smits <ruben dot smits at intermodalics dot eu>

# Version: 1.0
# Author: Ruben Smits <ruben dot smits at intermodalics dot eu>
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


from math import radians
from PyKDL import *
import sys
import unittest


class FrameVelTestFunctions(unittest.TestCase):
    def testdoubleVel(self):
        d = doubleVel()
        self.assertEqual(doubleVel(d), d)
        self.assertEqual(d.t, 0)
        self.assertEqual(d.grad, 0)
        self.assertEqual(d.value(), 0)
        self.assertEqual(d.deriv(), 0)
        d2 = -d
        self.assertEqual(d2.t, -d.t)
        self.assertEqual(d2.grad, -d.grad)

    def testVectorVel(self):
        v = VectorVel()
        vt = Vector()
        self.testVectorVelImpl(v, vt)
        vt = Vector(-4, -6, -8)
        self.testVectorVelImpl(v, vt)
        v1 = Vector(3, -4, 5)
        v2 = Vector(6, 3, -5)
        v = VectorVel(v1, v2)
        self.testVectorVelImpl(v, vt)

        # Members
        self.assertEqual(v.p, v1)
        self.assertEqual(v.v, v2)
        self.assertEqual(v.value(), v1)
        self.assertEqual(v.deriv(), v2)

        # Equality
        self.assertEqual(VectorVel(v).p, v.p)
        self.assertEqual(VectorVel(v).v, v.v)
        self.assertFalse(v == -v)  # Doesn't work for zero VectorVel
        self.assertFalse(Equal(v, -v))  # Doesn't work for zero VectorVel
        self.assertTrue(v != -v)  # Doesn't work for zero VectorVel
        self.assertTrue(not Equal(v, -v))  # Doesn't work for zero VectorVel

        v = VectorVel(v1)
        self.assertEqual(v, v1)
        self.assertEqual(v1, v)

        # Zero
        v = VectorVel(v1, v2)
        SetToZero(v)
        self.assertEqual(v, VectorVel())
        self.assertEqual(VectorVel.Zero(), VectorVel())

        # dot functions
        v = VectorVel(v1, v2)
        self.assertEqual(dot(v, v), doubleVel(dot(v.p, v.p), dot(v.p, v.v)+dot(v.v, v.p)))
        dot_result = doubleVel(dot(v.p, v1), dot(v.v, v1))
        self.assertEqual(dot(v, v1), dot_result)
        self.assertEqual(dot(v1, v), dot_result)

    def testVectorVelImpl(self, v, vt):
        self.assertEqual(2*v-v, v)
        self.assertEqual(v*2-v, v)
        self.assertEqual(v+v+v-2*v, v)
        v2 = VectorVel(v)
        self.assertEqual(v, v2)
        v2 += v
        self.assertEqual(2*v, v2)
        v2 -= v
        self.assertEqual(v, v2)
        v2.ReverseSign()
        self.assertEqual(v, -v2)
        self.assertEqual(v*vt, -vt*v)
        v2 = VectorVel(Vector(-5, -6, -3), Vector(3, 4, 5))
        self.assertEqual(v*v2, -v2*v)

    def testTwistVel(self):
        t = TwistVel()
        self.testTwistVelImpl(t)
        v1 = Vector(1, 2, 3)
        v2 = Vector(4, 5, 6)
        vv1 = VectorVel(v1, v2)
        vv2 = VectorVel(v2, v1)
        t = TwistVel(vv1, vv2)
        self.testTwistVelImpl(t)

        # Alternative constructor
        tw1 = Twist(v1, v2)
        tw2 = Twist(v2, v1)
        t2 = TwistVel(tw1, tw2)
        self.assertEqual(t, t2)

        # Members
        self.assertEqual(t.vel, vv1)
        self.assertEqual(t.rot, vv2)
        self.assertEqual(t2.value(), tw1)
        self.assertEqual(t2.deriv(), tw2)
        self.assertEqual(t2.GetTwist(), tw1)
        self.assertEqual(t2.GetTwistDot(), tw2)

        # Equality
        self.assertEqual(TwistVel(t).vel, t.vel)
        self.assertEqual(TwistVel(t).rot, t.rot)
        self.assertFalse(t == -t)  # Doesn't work for zero TwistVel
        self.assertFalse(Equal(t, -t))  # Doesn't work for zero TwistVel
        self.assertTrue(t != -t)  # Doesn't work for zero TwistVel
        self.assertTrue(not Equal(t, -t))  # Doesn't work for zero TwistVel

        t = TwistVel(VectorVel(v1), VectorVel(v2))
        t2 = Twist(v1, v2)
        self.assertEqual(t, t2)
        self.assertEqual(t2, t)

        # Zero
        SetToZero(t)
        self.assertEqual(t, TwistVel())
        self.assertEqual(TwistVel.Zero(), TwistVel())

    def testTwistVelImpl(self, t):
        self.assertEqual(2*t-t, t)
        self.assertEqual(t*2-t, t)
        self.assertEqual(t+t+t-2*t, t)
        t2 = TwistVel(t)
        self.assertEqual(t, t2)
        t2 += t
        self.assertEqual(2*t, t2)
        t2 -= t
        self.assertEqual(t, t2)
        t2.ReverseSign()
        self.assertEqual(t, -t2)
        self.assertEqual(t*doubleVel(), doubleVel()*t)
        self.assertEqual(t*doubleVel(5), doubleVel(5)*t)
        self.assertEqual(t * doubleVel(3, 5), doubleVel(3, 5) * t)

    def testRotationVel(self):
        v = VectorVel()
        vt = Vector()
        r = RotationVel()
        self.testRotationVelImpl(r, v, vt)
        v = VectorVel(Vector(9, 4, -2), Vector(-5, 6, -2))
        vt = Vector(2, 3, 4)
        rot = Rotation.RPY(radians(-15), radians(20), radians(-80))
        vec = Vector(2, 4, 1)
        r = RotationVel(rot, vec)
        self.testRotationVelImpl(r, v, vt)

        # Members
        self.assertEqual(r.R, rot)
        self.assertEqual(r.w, vec)
        self.assertEqual(r.value(), rot)
        self.assertEqual(r.deriv(), vec)

        # Equality
        self.assertEqual(RotationVel(r).R, r.R)
        self.assertEqual(RotationVel(r).w, r.w)
        self.assertEqual(RotationVel(rot), rot)
        self.assertEqual(rot, RotationVel(rot))

    def testRotationVelImpl(self, r, v, vt):
        r2 = RotationVel(r)
        self.assertEqual(r, r2)
        self.assertEqual((r*v).Norm(), v.Norm())
        self.assertEqual(r.Inverse(r*v), v)
        self.assertEqual(r*r.Inverse(v), v)
        self.assertEqual(r*Rotation.Identity(), r)
        self.assertEqual(Rotation.Identity()*r, r)
        self.assertEqual(r*(r*(r*v)), (r*r*r)*v)
        self.assertEqual(r*(r*(r*vt)), (r*r*r)*vt)
        self.assertEqual(r*r.Inverse(), RotationVel.Identity())
        self.assertEqual(r.Inverse()*r, RotationVel.Identity())
        self.assertEqual(r.Inverse()*v, r.Inverse(v))

    def testFrameVel(self):
        v = VectorVel()
        vt = Vector()
        f = FrameVel()
        self.testFrameVelImpl(f, v, vt)
        fr_m = Rotation.EulerZYX(radians(10), radians(20), radians(-10))
        fr_p = Vector(4, -2, 1)
        tw_vel = Vector(2, -2, -2)
        tw_rot = Vector(-5, -3, -2)
        fr = Frame(fr_m, fr_p)
        tw = Twist(tw_vel, tw_rot)
        f = FrameVel(fr, tw)
        self.testFrameVelImpl(f, v, vt)
        v = VectorVel(Vector(3, 4, 5), Vector(-2, -4, -1))
        vt = Vector(-1, 0, -10)
        self.testFrameVelImpl(f, v, vt)

        # Alternative constructor
        rv = RotationVel(fr_m, tw_rot)
        vv = VectorVel(fr_p, tw_vel)
        f2 = FrameVel(rv, vv)
        self.assertEqual(f, f2)

        # Members
        self.assertEqual(f.M, rv)
        self.assertEqual(f.p, vv)
        self.assertEqual(f2.value(), fr)
        self.assertEqual(f2.deriv(), tw)

        # Equality
        self.assertEqual(FrameVel(f).M, f.M)
        self.assertEqual(FrameVel(f).p, f.p)

        f = FrameVel(fr)
        self.assertEqual(f, fr)
        self.assertEqual(fr, f)

    def testFrameVelImpl(self, f, v, vt):
        f2 = FrameVel(f)
        self.assertEqual(f, f2)
        self.assertEqual(f.Inverse(f*v), v)
        self.assertEqual(f.Inverse(f*vt), vt)
        self.assertEqual(f*f.Inverse(v), v)
        self.assertEqual(f*f.Inverse(vt), vt)
        self.assertEqual(f*Frame.Identity(), f)
        self.assertEqual(Frame.Identity()*f, f)
        self.assertEqual(f*(f*(f*v)), (f*f*f)*v)
        self.assertEqual(f*(f*(f*vt)), (f*f*f)*vt)
        self.assertEqual(f*f.Inverse(), FrameVel.Identity())
        self.assertEqual(f.Inverse()*f, Frame.Identity())
        self.assertEqual(f.Inverse()*vt, f.Inverse(vt))

    def testPickle(self):
        if sys.version_info < (3, 0):
            import cPickle as pickle
        else:
            import pickle
        data = {}
        data['vv'] = VectorVel(Vector(1, 2, 3), Vector(4, 5, 6))
        data['rv'] = RotationVel(Rotation.RotX(1.3), Vector(4.1, 5.1, 6.1))
        data['fv'] = FrameVel(data['rv'], data['vv'])
        data['tv'] = TwistVel(data['vv'], data['vv'])

        with open('/tmp/pickle_test_kdl_framevel', 'wb') as f:
            pickle.dump(data, f, 2)

        with open('/tmp/pickle_test_kdl_framevel', 'rb') as f:
            data1 = pickle.load(f)

        self.assertEqual(data, data1)

    def testCopyImpl(self, copy):
        vv1 = VectorVel(Vector(1, 2, 3), Vector(4, 5, 6))
        vv2 = copy(vv1)
        self.assertEqual(vv1, vv2)
        rv1 = RotationVel(Rotation.RotX(1.3), Vector(4.1, 5.1, 6.1))
        rv2 = copy(rv1)
        self.assertEqual(rv1, rv2)
        fv1 = FrameVel(rv1, vv1)
        fv2 = copy(fv1)
        self.assertEqual(fv1, fv2)
        tv1 = TwistVel(vv1, vv1)
        tv2 = copy(tv1)
        self.assertEqual(tv1, tv2)

    def testCopy(self):
        from copy import copy
        self.testCopyImpl(copy)

    def testDeepCopy(self):
        from copy import deepcopy
        self.testCopyImpl(deepcopy)


def suite():
    suite = unittest.TestSuite()
    suite.addTest(FrameVelTestFunctions('testdoubleVel'))
    suite.addTest(FrameVelTestFunctions('testVectorVel'))
    suite.addTest(FrameVelTestFunctions('testTwistVel'))
    suite.addTest(FrameVelTestFunctions('testRotationVel'))
    suite.addTest(FrameVelTestFunctions('testFrameVel'))
    suite.addTest(FrameVelTestFunctions('testPickle'))
    suite.addTest(FrameVelTestFunctions('testCopy'))
    suite.addTest(FrameVelTestFunctions('testDeepCopy'))
    return suite


if __name__ == '__main__':
    suite = suite()
    result = unittest.TextTestRunner(verbosity=3).run(suite)

    if result.wasSuccessful():
        sys.exit(0)
    else:
        sys.exit(1)
