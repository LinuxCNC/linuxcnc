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


from builtins import range

import gc
import psutil
from PyKDL import *
import random
import sys
import unittest


def changeJacRepresentation(J, F_bs_ee, representation):
        if representation == ChainJntToJacDotSolver.HYBRID:
            pass
        elif representation == ChainJntToJacDotSolver.BODYFIXED:
            J.changeBase(F_bs_ee.M.Inverse())
        elif representation == ChainJntToJacDotSolver.INERTIAL:
            J.changeRefPoint(-F_bs_ee.p)


def Jdot_diff(J_q, J_qdt, dt, Jdot):
    assert J_q.columns() == J_qdt.columns()
    assert J_q.columns() == Jdot.columns()
    for l in range(J_q.rows()):
        for c in range(J_q.columns()):
            Jdot[l, c] = (J_qdt[l, c] - J_q[l, c]) / dt


class KinfamTestFunctions(unittest.TestCase):
    def setUp(self):
        self.chain = Chain()
        self.chain.addSegment(Segment(Joint(Joint.RotZ),
                                      Frame(Vector(0.0, 0.0, 0.0))))
        self.chain.addSegment(Segment(Joint(Joint.RotX),
                                      Frame(Vector(0.0, 0.0, 0.9))))
        self.chain.addSegment(Segment(Joint(Joint.Fixed),
                                      Frame(Vector(-0.4, 0.0, 0.0))))
        self.chain.addSegment(Segment(Joint(Joint.RotY),
                                      Frame(Vector(0.0, 0.0, 1.2))))
        self.chain.addSegment(Segment(Joint(Joint.Fixed),
                                      Frame(Vector(0.4, 0.0, 0.0))))
        self.chain.addSegment(Segment(Joint(Joint.TransZ),
                                      Frame(Vector(0.0, 0.0, 1.4))))
        self.chain.addSegment(Segment(Joint(Joint.TransX),
                                      Frame(Vector(0.0, 0.0, 0.0))))
        self.chain.addSegment(Segment(Joint(Joint.TransY),
                                      Frame(Vector(0.0, 0.0, 0.4))))
        self.chain.addSegment(Segment(Joint(Joint.Fixed),
                                      Frame(Vector(0.0, 0.0, 0.0))))

        self.jacsolver = ChainJntToJacSolver(self.chain)
        self.jacdotsolver = ChainJntToJacDotSolver(self.chain)
        self.fksolverpos = ChainFkSolverPos_recursive(self.chain)
        self.fksolvervel = ChainFkSolverVel_recursive(self.chain)
        self.iksolvervel = ChainIkSolverVel_pinv(self.chain)
        self.iksolvervel_givens = ChainIkSolverVel_pinv_givens(self.chain)
        self.iksolverpos = ChainIkSolverPos_NR(self.chain, self.fksolverpos, self.iksolvervel)
        self.iksolverpos_givens = ChainIkSolverPos_NR(self.chain, self.fksolverpos, self.iksolvervel_givens)

    def testRotationalInertia(self):
        ri = RotationalInertia(1, 2, 3, 4, 7, 5)
        # __getitem__
        for i in range(3):
            for j in range(3):
                self.assertEqual(ri[3*i+j], 2*abs(i-j) + max(i, j) + 1)
        with self.assertRaises(IndexError):
            _ = ri[-1]
        with self.assertRaises(IndexError):
            _ = ri[9]

        # __setitem__
        for i in range(3):
            for j in range(3):
                ri[i] = i
        for i in range(3):
            for j in range(3):
                self.assertEqual(ri[i], i)
        with self.assertRaises(IndexError):
            ri[-1] = 1
        with self.assertRaises(IndexError):
            ri[9] = 1

    def testJacobian(self):
        jac = Jacobian(3)
        for i in range(jac.columns()):
            jac.setColumn(i, Twist(Vector(6*i+1, 6*i+2, 6*i+3), Vector(6*i+4, 6*i+5, 6*i+6)))
        # __getitem__
        for i in range(6):
            for j in range(3):
                self.assertEqual(jac[i, j], 6*j+i+1)
        with self.assertRaises(IndexError):
            _ = jac[-1, 0]
        with self.assertRaises(IndexError):
            _ = jac[6, 0]
        with self.assertRaises(IndexError):
            _ = jac[5, -1]
        with self.assertRaises(IndexError):
            _ = jac[5, 3]

        # __setitem__
        for i in range(6):
            for j in range(3):
                jac[i, j] = 3*i+j
        for i in range(6):
            for j in range(3):
                self.assertEqual(jac[i, j], 3*i+j)
        with self.assertRaises(IndexError):
            jac[-1, 0] = 1
        with self.assertRaises(IndexError):
            jac[6, 0] = 1
        with self.assertRaises(IndexError):
            jac[5, -1] = 1
        with self.assertRaises(IndexError):
            jac[5, 3] = 1

    def testJntArray(self):
        ja = JntArray(3)
        # __getitem__
        for i in range(3):
            self.assertEqual(ja[i], 0)
        with self.assertRaises(IndexError):
            _ = ja[-1]
        with self.assertRaises(IndexError):
            _ = ja[3]

        # __setitem__
        for i in range(3):
            ja[i] = 2*i
        for i in range(3):
            self.assertEqual(ja[i], 2*i)
        with self.assertRaises(IndexError):
            ja[-1] = 1
        with self.assertRaises(IndexError):
            ja[3] = 1

    def testFkPosAndJac(self):
        deltaq = 1E-4
        epsJ = 1E-4

        F1 = Frame()
        F2 = Frame()

        q = JntArray(self.chain.getNrOfJoints())
        jac = Jacobian(self.chain.getNrOfJoints())

        for i in range(q.rows()):
            q[i] = random.uniform(-3.14, 3.14)

        self.jacsolver.JntToJac(q, jac)

        for i in range(q.rows()):
            oldqi = q[i]
            q[i] = oldqi+deltaq
            self.assertTrue(0 == self.fksolverpos.JntToCart(q, F2))
            q[i] = oldqi-deltaq
            self.assertTrue(0 == self.fksolverpos.JntToCart(q, F1))
            q[i] = oldqi
            Jcol1 = diff(F1, F2, 2*deltaq)
            Jcol2 = Twist(Vector(jac[0, i], jac[1, i], jac[2, i]),
                          Vector(jac[3, i], jac[4, i], jac[5, i]))
            self.assertEqual(Jcol1, Jcol2)

    def testFkVelAndJac(self):
        deltaq = 1E-4
        epsJ = 1E-4

        q = JntArray(self.chain.getNrOfJoints())
        qdot = JntArray(self.chain.getNrOfJoints())
        for i in range(q.rows()):
            q[i] = random.uniform(-3.14, 3.14)
            qdot[i] = random.uniform(-3.14, 3.14)

        qvel = JntArrayVel(q, qdot)
        jac = Jacobian(self.chain.getNrOfJoints())

        cart = FrameVel.Identity()
        t = Twist.Zero()

        self.jacsolver.JntToJac(qvel.q, jac)
        self.assertTrue(self.fksolvervel.JntToCart(qvel, cart) == 0)
        MultiplyJacobian(jac, qvel.qdot, t)
        self.assertEqual(cart.deriv(), t)

    def testFkVelAndIkVelImpl(self, fksolvervel, iksolvervel, epsJ):
        q = JntArray(self.chain.getNrOfJoints())
        qdot = JntArray(self.chain.getNrOfJoints())
        for i in range(q.rows()):
            q[i] = random.uniform(-3.14, 3.14)
            qdot[i] = random.uniform(-3.14, 3.14)

        qvel = JntArrayVel(q, qdot)
        qdot_solved = JntArray(self.chain.getNrOfJoints())

        cart = FrameVel()

        self.assertTrue(0 == fksolvervel.JntToCart(qvel, cart))
        self.assertTrue(0 == iksolvervel.CartToJnt(qvel.q, cart.deriv(), qdot_solved))

        self.assertEqual(qvel.qdot, qdot_solved)

    def testFkVelAndIkVel(self):
        epsJ = 1e-7
        self.testFkVelAndIkVelImpl(self.fksolvervel, self.iksolvervel, epsJ)

    def testFkVelAndIkVelGivens(self):
        epsJ = 1e-7
        self.testFkVelAndIkVelImpl(self.fksolvervel, self.iksolvervel_givens, epsJ)

    def testFkPosAndIkPosImpl(self, fksolverpos, iksolverpos, epsJ):
        q = JntArray(self.chain.getNrOfJoints())
        for i in range(q.rows()):
            q[i] = random.uniform(-3.14, 3.14)

        q_init = JntArray(self.chain.getNrOfJoints())
        for i in range(q_init.rows()):
            q_init[i] = q[i]+0.1*random.random()

        q_solved = JntArray(q.rows())

        F1 = Frame.Identity()
        F2 = Frame.Identity()

        self.assertTrue(0 == fksolverpos.JntToCart(q, F1))
        self.assertTrue(0 == iksolverpos.CartToJnt(q_init, F1, q_solved))
        self.assertTrue(0 == fksolverpos.JntToCart(q_solved, F2))

        self.assertEqual(F1, F2)
        self.assertTrue(Equal(q, q_solved, epsJ), "{} != {}".format(q, q_solved))

    def testFkPosAndIkPos(self):
        epsJ = 1e-3
        self.testFkPosAndIkPosImpl(self.fksolverpos, self.iksolverpos, epsJ)

    def testFkPosAndIkPosGivens(self):
        epsJ = 1e-3
        self.testFkPosAndIkPosImpl(self.fksolverpos, self.iksolverpos_givens, epsJ)

    def compare_Jdot_Diff_vs_Solver(self, dt, representation):
        NrOfJoints = self.chain.getNrOfJoints()
        q = JntArray(NrOfJoints)
        qdot = JntArray(NrOfJoints)
        for i in range(q.rows()):
            q[i] = random.random()
            qdot[i] = random.random()

        q_dqdt = JntArray(q)
        for i in range(q.rows()):
            q_dqdt[i] += dt * qdot[i]
        
        F_bs_ee_q = Frame.Identity()
        F_bs_ee_q_dqdt = Frame.Identity()

        jac_q = Jacobian(NrOfJoints)
        jac_q_dqdt = Jacobian(NrOfJoints)
        jdot_by_diff = Jacobian(NrOfJoints)

        self.jacsolver.JntToJac(q, jac_q)
        self.jacsolver.JntToJac(q_dqdt, jac_q_dqdt)

        self.fksolverpos.JntToCart(q, F_bs_ee_q)
        self.fksolverpos.JntToCart(q_dqdt, F_bs_ee_q_dqdt)

        changeJacRepresentation(jac_q, F_bs_ee_q, representation)
        changeJacRepresentation(jac_q_dqdt, F_bs_ee_q_dqdt, representation)
        
        Jdot_diff(jac_q, jac_q_dqdt, dt, jdot_by_diff)

        jdot_by_solver = Jacobian(NrOfJoints)
        self.jacdotsolver.setRepresentation(representation)
        self.jacdotsolver.JntToJacDot(JntArrayVel(q_dqdt, qdot), jdot_by_solver)
        
        jdot_qdot_by_solver = Twist()
        MultiplyJacobian(jdot_by_solver, qdot, jdot_qdot_by_solver)

        jdot_qdot_by_diff = Twist()
        MultiplyJacobian(jdot_by_diff, qdot, jdot_qdot_by_diff)

        err = (jdot_qdot_by_diff.vel.Norm() - jdot_qdot_by_solver.vel.Norm()
               + jdot_qdot_by_diff.rot.Norm() - jdot_qdot_by_solver.rot.Norm())
        return abs(err), jdot_qdot_by_solver, jdot_qdot_by_diff

    def testJacDot(self):
        dt = 1e-6
        success = True
        while dt <= 0.1:
            eps_diff_vs_solver = 4*dt
            representations = [ChainJntToJacDotSolver.HYBRID,
                               ChainJntToJacDotSolver.INERTIAL,
                               ChainJntToJacDotSolver.BODYFIXED]

            for representation in representations:
                err, jdot_qdot_solver, jdot_qdot_diff = self.compare_Jdot_Diff_vs_Solver(dt, representation)
                success &= err <= eps_diff_vs_solver
                self.assertTrue(success, "{} != {}\n"
                                         "representation: {}\n"
                                         "dt: {}\n"
                                         "eps_diff_vs_solver: {}\n"
                                         "err: {}".format(jdot_qdot_solver, jdot_qdot_diff, representation, dt,
                                                          eps_diff_vs_solver, err))
            dt *= 10
        

class KinfamTestTree(unittest.TestCase):

    def setUp(self):
        self.tree = Tree()
        self.tree.addSegment(Segment(Joint(Joint.RotZ),
                                     Frame(Vector(0.0, 0.0, 0.0))), "foo")
        self.tree.addSegment(Segment(Joint(Joint.Fixed),
                                     Frame(Vector(0.0, 0.0, 0.0))), "bar")

    def testTreeGetChainMemLeak(self):
        # test for the memory leak in Tree.getChain described in issue #211
        process = psutil.Process()
        self.tree.getChain("foo", "bar")
        gc.collect()
        mem_before = process.memory_info().vms
        # needs at least 2000 iterations on my system to cause a detectable
        # difference in memory usage
        for _ in range(10000):
            self.tree.getChain("foo", "bar")
        gc.collect()
        mem_after = process.memory_info().vms
        self.assertEqual(mem_before, mem_after)


def suite():
    suite = unittest.TestSuite()
    suite.addTest(KinfamTestFunctions('testRotationalInertia'))
    suite.addTest(KinfamTestFunctions('testJacobian'))
    suite.addTest(KinfamTestFunctions('testJntArray'))
    suite.addTest(KinfamTestFunctions('testFkPosAndJac'))
    suite.addTest(KinfamTestFunctions('testFkVelAndJac'))
    suite.addTest(KinfamTestFunctions('testFkVelAndIkVel'))
    suite.addTest(KinfamTestFunctions('testFkVelAndIkVelGivens'))
    suite.addTest(KinfamTestFunctions('testFkPosAndIkPos'))
    suite.addTest(KinfamTestFunctions('testFkPosAndIkPosGivens'))
    suite.addTest(KinfamTestFunctions('testJacDot'))
    suite.addTest(KinfamTestTree('testTreeGetChainMemLeak'))
    return suite


if __name__ == '__main__':
    suite = suite()
    result = unittest.TextTestRunner(verbosity=3).run(suite)

    if result.wasSuccessful():
        sys.exit(0)
    else:
        sys.exit(1)
