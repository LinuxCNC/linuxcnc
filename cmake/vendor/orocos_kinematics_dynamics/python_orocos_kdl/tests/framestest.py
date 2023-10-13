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


from math import radians, sqrt
from PyKDL import *
import sys
import unittest


class FramesTestFunctions(unittest.TestCase):
    def testVector(self):
        v = Vector(3, 4, 5)
        self.testVectorImpl(v)
        v = Vector()
        self.testVectorImpl(v)

        # Equality
        v = Vector(3, 4, 5)
        self.assertFalse(v == -v)  # Doesn't work for zero vector
        self.assertFalse(Equal(v, -v))  # Doesn't work for zero vector
        self.assertTrue(v != -v)  # Doesn't work for zero vector
        self.assertTrue(not Equal(v, -v))  # Doesn't work for zero vector

        # Test member get and set functions
        self.assertEqual(v.x(), 3)
        v.x(1)
        self.assertEqual(v, Vector(1, 4, 5))
        self.assertEqual(v.y(), 4)
        v.y(1)
        self.assertEqual(v, Vector(1, 1, 5))
        self.assertEqual(v.z(), 5)
        v.z(1)
        self.assertEqual(v, Vector(1, 1, 1))

        # __getitem__
        self.assertEqual(v[0], 1)
        self.assertEqual(v[1], 1)
        self.assertEqual(v[2], 1)
        with self.assertRaises(IndexError):
            _ = v[-1]
        with self.assertRaises(IndexError):
            _ = v[3]

        # __setitem__
        v[0] = 3
        self.assertEqual(v[0], 3)
        v[1] = 4
        self.assertEqual(v[1], 4)
        v[2] = 5
        self.assertEqual(v[2], 5)
        with self.assertRaises(IndexError):
            v[-1] = 1
        with self.assertRaises(IndexError):
            v[3] = 1

        # Zero
        SetToZero(v)
        self.assertEqual(v, Vector(0, 0, 0))
        self.assertEqual(Vector.Zero(), Vector(0, 0, 0))

    def testVectorImpl(self, v):
        self.assertTrue(v == v)
        self.assertTrue(Equal(v, v))
        self.assertFalse(v != v)
        self.assertFalse(not Equal(v, v))
        self.assertEqual(2*v-v, v)
        self.assertEqual(v*2-v, v)
        self.assertEqual(v+v+v-2*v, v)
        v2 = Vector(v)
        self.assertEqual(v, v2)
        v2 += v
        self.assertEqual(2*v, v2)
        v2 -= v
        self.assertEqual(v, v2)
        v2.ReverseSign()
        self.assertEqual(v, -v2)
        for v2 in [Vector(v), -Vector(v)]:
            self.assertAlmostEqual(v2.Norm()**2, sum(map(lambda i: i * i, v2)), delta=1e-10)
            self.assertEqual(v2.Norm(), v2.Normalize())  # Norm before Normalize, so taking norm of un-normalized vector

        self.assertEqual(dot(v, v), sum(map(lambda i: i * i, v)))

    def testTwist(self):
        t = Twist(Vector(6, 3, 5), Vector(4, -2, 7))
        self.testTwistImpl(t)
        t = Twist()
        self.testTwistImpl(t)
        t = Twist(Vector(0, -9, -3), Vector(1, -2, -4))
        self.testTwistImpl(t)

        # Equality
        t = Twist(Vector(1, 2, 3), Vector(1, 2, 3))
        self.assertFalse(t == -t)  # Doesn't work for zero twist
        self.assertFalse(Equal(t, -t))  # Doesn't work for zero twist
        self.assertTrue(t != -t)  # Doesn't work for zero twist
        self.assertTrue(not Equal(t, -t))  # Doesn't work for zero twist

        # Members
        v1 = Vector(1, 2, 3)
        v2 = Vector(4, 5, 6)
        t = Twist(v1, v2)
        self.assertEqual(t.vel, v1)
        self.assertEqual(t.rot, v2)
        self.assertEqual(Twist(t).vel, t.vel)
        self.assertEqual(Twist(t).rot, t.rot)

        # __getitem__
        for i in range(6):
            self.assertEqual(t[i], i+1)
        with self.assertRaises(IndexError):
            _ = t[-1]
        with self.assertRaises(IndexError):
            _ = t[6]

        # __setitem__
        for i in range(6):
            t[i] = i
        for i in range(6):
            self.assertEqual(t[i], i)
        with self.assertRaises(IndexError):
            t[-1] = 1
        with self.assertRaises(IndexError):
            t[6] = 1

        # Zero
        SetToZero(t)
        self.assertEqual(t, Twist())
        self.assertEqual(Twist.Zero(), Twist())

    def testTwistImpl(self, t):
        self.assertTrue(t == t)
        self.assertTrue(Equal(t, t))
        self.assertFalse(t != t)
        self.assertFalse(not Equal(t, t))
        self.assertEqual(2*t-t, t)
        self.assertEqual(t*2-t, t)
        self.assertEqual(t+t+t-2*t, t)
        t2 = Twist(t)
        self.assertEqual(t, t2)
        t2 += t
        self.assertEqual(2*t, t2)
        t2 -= t
        self.assertEqual(t, t2)
        t2.ReverseSign()
        self.assertEqual(t, -t2)
        v = Vector(1, 2, 3)
        t2 = t.RefPoint(v)
        self.assertEqual(t2.vel, t.vel + t.rot*v)
        self.assertEqual(t2.rot, t.rot)

        # No need to test the dot functions again for Wrench
        w = Wrench(v, v)
        dot_result = dot(t.vel, w.force) + dot(t.rot, w.torque)
        self.assertEqual(dot(t, w), dot_result)
        self.assertEqual(dot(w, t), dot_result)

    def testWrench(self):
        w = Wrench(Vector(7, -1, 3), Vector(2, -3, 3))
        self.testWrenchImpl(w)
        w = Wrench()
        self.testWrenchImpl(w)
        w = Wrench(Vector(2, 1, 4), Vector(5, 3, 1))
        self.testWrenchImpl(w)

        # Equality
        w = Wrench(Vector(1, 2, 3), Vector(1, 2, 3))
        self.assertFalse(w == -w)  # Doesn't work for zero wrench
        self.assertFalse(Equal(w, -w))  # Doesn't work for zero wrench
        self.assertTrue(w != -w)  # Doesn't work for zero wrench
        self.assertTrue(not Equal(w, -w))  # Doesn't work for zero wrench

        # Members
        v1 = Vector(1, 2, 3)
        v2 = Vector(4, 5, 6)
        w = Wrench(v1, v2)
        self.assertEqual(w.force, v1)
        self.assertEqual(w.torque, v2)
        self.assertEqual(Wrench(w).force, w.force)
        self.assertEqual(Wrench(w).torque, w.torque)

        # __getitem__
        for i in range(6):
            self.assertEqual(w[i], i+1)
        with self.assertRaises(IndexError):
            _ = w[-1]
        with self.assertRaises(IndexError):
            _ = w[6]

        # __setitem__
        for i in range(6):
            w[i] = i
        for i in range(6):
            self.assertEqual(w[i], i)
        with self.assertRaises(IndexError):
            w[-1] = 1
        with self.assertRaises(IndexError):
            w[6] = 1

        # Zero
        SetToZero(w)
        self.assertEqual(w, Wrench())
        self.assertEqual(Wrench.Zero(), Wrench())

    def testWrenchImpl(self, w):
        self.assertEqual(2*w-w, w)
        self.assertEqual(w*2-w, w)
        self.assertEqual(w+w+w-2*w, w)
        w2 = Wrench(w)
        self.assertEqual(w, w2)
        w2 += w
        self.assertEqual(2*w, w2)
        w2 -= w
        self.assertEqual(w, w2)
        w2.ReverseSign()
        self.assertEqual(w, -w2)

        v = Vector(1, 2, 3)
        w2 = w.RefPoint(v)
        self.assertEqual(w2.force, w.force)
        self.assertEqual(w2.torque, w.torque + w.force*v)

    def testRotation(self):
        self.testRotationImpl(Rotation.RPY(radians(10), radians(20), radians(30)), Vector(3, 4, 5))
        self.testRotationImpl(Rotation.RPY(radians(10), radians(20), radians(30)), Vector())
        self.testRotationImpl(Rotation(), Vector(3, 4, 5))
        self.testRotationImpl(Rotation(), Vector())

        r = Rotation(*range(1, 10))

        # __getitem__
        for i in range(3):
            for j in range(3):
                self.assertEqual(r[i, j], 3*i+j+1)
        with self.assertRaises(IndexError):
            _ = r[-1, 0]
        with self.assertRaises(IndexError):
            _ = r[0, -1]
        with self.assertRaises(IndexError):
            _ = r[3, 2]
        with self.assertRaises(IndexError):
            _ = r[2, 3]

        # __setitem__
        for i in range(3):
            for j in range(3):
                r[i, j] = 3*i+j
        for i in range(3):
            for j in range(3):
                self.assertEqual(r[i, j], 3*i+j)
        with self.assertRaises(IndexError):
            r[-1, 0] = 1
        with self.assertRaises(IndexError):
            r[0, -1] = 1
        with self.assertRaises(IndexError):
            r[3, 2] = 1
        with self.assertRaises(IndexError):
            r[2, 3] = 1

    def testRotationImpl(self, r, v):
        w = Wrench(Vector(7, -1, 3), Vector(2, -3, 3))
        t = Twist(Vector(6, 3, 5), Vector(4, -2, 7))
        a, b, c = r.GetRPY()

        self.assertAlmostEqual(dot(r.UnitX(), r.UnitX()), 1.0, 15)
        self.assertEqual(dot(r.UnitY(), r.UnitY()), 1.0)
        self.assertEqual(dot(r.UnitZ(), r.UnitZ()), 1.0)
        self.assertAlmostEqual(dot(r.UnitX(), r.UnitY()), 0.0, 15)
        self.assertAlmostEqual(dot(r.UnitX(), r.UnitZ()), 0.0, 15)
        self.assertAlmostEqual(dot(r.UnitY(), r.UnitZ()), 0.0, 15)
        r2 = Rotation(r)
        self.assertEqual(r, r2)
        self.assertAlmostEqual((r*v).Norm(), v.Norm(), 14)
        self.assertEqual(r.Inverse(r*v), v)
        self.assertEqual(r.Inverse(r*t), t)
        self.assertEqual(r.Inverse(r*w), w)
        self.assertEqual(r*r.Inverse(v), v)
        self.assertEqual(r*Rotation.Identity(), r)
        self.assertEqual(Rotation.Identity()*r, r)
        self.assertEqual(r*(r*(r*v)), (r*r*r)*v)
        self.assertEqual(r*(r*(r*t)), (r*r*r)*t)
        self.assertEqual(r*(r*(r*w)), (r*r*r)*w)
        self.assertEqual(r*r.Inverse(), Rotation.Identity())
        self.assertEqual(r.Inverse()*r, Rotation.Identity())
        self.assertEqual(r.Inverse()*v, r.Inverse(v))
        (ra, rb, rc) = r.GetRPY()
        self.assertEqual(ra, a)
        self.assertEqual(rb, b)
        self.assertEqual(rc, c)
        r = Rotation.EulerZYX(a, b, c)
        (ra, rb, rc) = r.GetEulerZYX()
        self.assertEqual(ra, a)
        self.assertEqual(rb, b)
        self.assertEqual(rc, c)
        r = Rotation.EulerZYZ(a, b, c)
        (ra, rb, rc) = r.GetEulerZYZ()
        self.assertEqual(ra, a)
        self.assertEqual(rb, b)
        self.assertAlmostEqual(rc, c, 15)
        (angle, v2) = r.GetRotAngle()
        r2 = Rotation.Rot(v2, angle)
        self.assertEqual(r2, r)
        r2 = Rotation.Rot(v2*1E20, angle)
        self.assertEqual(r, r2)
        v2 = Vector(6, 2, 4)
        self.assertAlmostEqual(v2.Norm(), sqrt(dot(v2, v2)), 14)

    def testFrame(self):
        v = Vector(3, 4, 5)
        w = Wrench(Vector(7, -1, 3), Vector(2, -3, 3))
        t = Twist(Vector(6, 3, 5), Vector(4, -2, 7))
        f = Frame()
        self.testFrameImpl(v, w, t, f)
        r = Rotation.EulerZYX(radians(10), radians(20), radians(-10))
        v2 = Vector(4, -2, 1)
        f = Frame(r, v2)
        self.testFrameImpl(v, w, t, f)

        # Equality
        f2 = Frame(f)
        self.assertEqual(f, f2)

        # Members
        self.assertEqual(f.M, r)
        self.assertEqual(f.p, v2)
        self.assertEqual(Frame(f).M, f.M)
        self.assertEqual(Frame(f).p, f.p)

        f = Frame(Rotation(1, 2, 3,
                           5, 6, 7,
                           9, 10, 11),
                  Vector(4, 8, 12))
        # __getitem__
        for i in range(3):
            for j in range(4):
                self.assertEqual(f[i, j], 4*i+j+1)
        with self.assertRaises(IndexError):
            _ = f[-1, 0]
        with self.assertRaises(IndexError):
            _ = f[0, -1]
        with self.assertRaises(IndexError):
            _ = f[3, 3]
        with self.assertRaises(IndexError):
            _ = f[2, 4]

        # __setitem__
        for i in range(3):
            for j in range(4):
                f[i, j] = 4*i+j
        for i in range(3):
            for j in range(4):
                self.assertEqual(f[i, j], 4*i+j)
        with self.assertRaises(IndexError):
            f[-1, 0] = 1
        with self.assertRaises(IndexError):
            f[0, -1] = 1
        with self.assertRaises(IndexError):
            f[3, 3] = 1
        with self.assertRaises(IndexError):
            f[2, 4] = 1

    def testFrameImpl(self, v, w, t, f):
        self.assertEqual(f.Inverse(f*v), v)
        self.assertEqual(f.Inverse(f*t), t)
        self.assertEqual(f.Inverse(f*w), w)
        self.assertEqual(f*f.Inverse(v), v)
        self.assertEqual(f*f.Inverse(t), t)
        self.assertEqual(f*f.Inverse(w), w)
        self.assertEqual(f*Frame.Identity(), f)
        self.assertEqual(Frame.Identity()*f, f)
        self.assertEqual(f*(f*(f*v)), (f*f*f)*v)
        self.assertEqual(f*(f*(f*t)), (f*f*f)*t)
        self.assertEqual(f*(f*(f*w)), (f*f*f)*w)
        self.assertEqual(f*f.Inverse(), Frame.Identity())
        self.assertEqual(f.Inverse()*f, Frame.Identity())
        self.assertEqual(f.Inverse()*v, f.Inverse(v))

    def testPickle(self):
        if sys.version_info < (3, 0):
            import cPickle as pickle
        else:
            import pickle
        data = dict()
        data['v'] = Vector(1, 2, 3)
        data['rot'] = Rotation().EulerZYZ(1, 2, 3)
        data['fr'] = Frame(data['rot'], data['v'])
        data['tw'] = Twist(data['v'], Vector(4, 5, 6))
        data['wr'] = Wrench(Vector(0.1, 0.2, 0.3), data['v'])

        with open('/tmp/pickle_test', 'wb') as f:
            pickle.dump(data, f, 2)

        with open('/tmp/pickle_test', 'rb') as f:
            data1 = pickle.load(f)

        self.assertEqual(data, data1)

    def testCopyImpl(self, copy):
        v1 = Vector(1, 2, 3)
        v2 = copy(v1)
        self.assertEqual(v1, v2)
        r1 = Rotation().EulerZYZ(1, 2, 3)
        r2 = copy(r1)
        self.assertEqual(r1, r2)
        f1 = Frame(r1, v1)
        f2 = copy(f1)
        self.assertEqual(f1, f2)
        t1 = Twist(v1, Vector(4, 5, 6))
        t2 = copy(t1)
        self.assertEqual(t1, t2)
        w1 = Wrench(Vector(0.1, 0.2, 0.3), v1)
        w2 = copy(w1)
        self.assertEqual(w1, w2)

    def testCopy(self):
        from copy import copy
        self.testCopyImpl(copy)

    def testDeepCopy(self):
        from copy import deepcopy
        self.testCopyImpl(deepcopy)


def suite():
    suite = unittest.TestSuite()
    suite.addTest(FramesTestFunctions('testVector'))
    suite.addTest(FramesTestFunctions('testTwist'))
    suite.addTest(FramesTestFunctions('testWrench'))
    suite.addTest(FramesTestFunctions('testRotation'))
    suite.addTest(FramesTestFunctions('testFrame'))
    suite.addTest(FramesTestFunctions('testPickle'))
    suite.addTest(FramesTestFunctions('testCopy'))
    suite.addTest(FramesTestFunctions('testDeepCopy'))
    return suite


if __name__ == '__main__':
    suite = suite()
    result = unittest.TextTestRunner(verbosity=3).run(suite)

    if result.wasSuccessful():
        sys.exit(0)
    else:
        sys.exit(1)
