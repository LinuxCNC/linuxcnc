//Copyright  (C)  2020  Ruben Smits <ruben dot smits at intermodalics dot eu>
//
//Version: 1.0
//Author: Ruben Smits Ruben Smits <ruben dot smits at intermodalics dot eu>
//Author: Zihan Chen <zihan dot chen dot jhu at gmail dot com>
//Author: Matthijs van der Burgh <MatthijsBurgh at outlook dot com>
//Maintainer: Ruben Smits Ruben Smits <ruben dot smits at intermodalics dot eu>
//URL: http://www.orocos.org/kdl
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


#include <kdl/joint.hpp>
#include <kdl/rotationalinertia.hpp>
#include <kdl/rigidbodyinertia.hpp>
#include <kdl/chain.hpp>
#include <kdl/tree.hpp>
#include <kdl/jntarrayvel.hpp>
#include <kdl/chainiksolver.hpp>
#include <kdl/chainfksolver.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/chainfksolvervel_recursive.hpp>
#include <kdl/chainiksolverpos_nr.hpp>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/chainiksolvervel_wdls.hpp>
#include <kdl/chainiksolverpos_lma.hpp>
#include <kdl/chainiksolvervel_pinv_nso.hpp>
#include <kdl/chainiksolvervel_pinv_givens.hpp>
#include <kdl/chainjnttojacsolver.hpp>
#include <kdl/chainjnttojacdotsolver.hpp>
#include <kdl/chainidsolver_recursive_newton_euler.hpp>
#include <kdl/kinfam_io.hpp>
#include "PyKDL.h"

namespace py = pybind11;
using namespace KDL;


void init_kinfam(pybind11::module &m)
{
    // --------------------
    // Joint
    // --------------------
    py::class_<Joint> joint(m, "Joint");
    py::enum_<Joint::JointType> joint_type(joint, "JointType");
        joint_type.value("RotAxis", Joint::JointType::RotAxis);
        joint_type.value("RotX", Joint::JointType::RotX);
        joint_type.value("RotY", Joint::JointType::RotY);
        joint_type.value("RotZ", Joint::JointType::RotZ);
        joint_type.value("TransAxis", Joint::JointType::TransAxis);
        joint_type.value("TransX", Joint::JointType::TransX);
        joint_type.value("TransY", Joint::JointType::TransY);
        joint_type.value("TransZ", Joint::JointType::TransZ);
        joint_type.value("Fixed", Joint::JointType::Fixed);
#if PY_VERSION_HEX < 0x03000000
        joint_type.value("None", Joint::JointType::None);
#endif
        joint_type.export_values();

    joint.def(py::init<>());
    joint.def(py::init<std::string, Joint::JointType, double, double, double, double, double>(),
              py::arg("name"), py::arg("type")=Joint::JointType::Fixed, py::arg("scale")=1, py::arg("offset")=0,
              py::arg("inertia")=0, py::arg("damping")=0, py::arg("stiffness")=0);
    joint.def(py::init<Joint::JointType, double, double, double, double, double>(),
              py::arg("type")=Joint::JointType::Fixed, py::arg("scale")=1, py::arg("offset")=0,
              py::arg("inertia")=0, py::arg("damping")=0, py::arg("stiffness")=0);
    joint.def(py::init<std::string, Vector, Vector, Joint::JointType, double, double, double, double, double>(),
              py::arg("name"), py::arg("origin"), py::arg("axis"), py::arg("type"), py::arg("scale")=1, py::arg("offset")=0,
              py::arg("inertia")=0, py::arg("damping")=0, py::arg("stiffness")=0);
    joint.def(py::init<Vector, Vector, Joint::JointType, double, double, double, double, double>(),
              py::arg("origin"), py::arg("axis"), py::arg("type"), py::arg("scale")=1, py::arg("offset")=0,
              py::arg("inertia")=0, py::arg("damping")=0, py::arg("stiffness")=0);
    joint.def(py::init<const Joint&>());
    joint.def("pose", &Joint::pose);
    joint.def("twist", &Joint::twist);
    joint.def("JointAxis", &Joint::JointAxis);
    joint.def("JointOrigin", &Joint::JointOrigin);
    joint.def("getName", &Joint::getName);
    joint.def("getType", &Joint::getType);
    joint.def("getTypeName", &Joint::getTypeName);
    joint.def("__repr__", [](const Joint &j)
    {
        std::ostringstream oss;
        oss << j;
        return oss.str();
    });


    // --------------------
    // RotationalInertia
    // --------------------
    py::class_<RotationalInertia> rotational_inertia(m, "RotationalInertia");
    rotational_inertia.def(py::init<double, double, double, double, double, double>(),
                           py::arg("Ixx")=0, py::arg("Iyy")=0, py::arg("Izz")=0,
                           py::arg("Ixy")=0, py::arg("Ixz")=0, py::arg("Iyz")=0);
    rotational_inertia.def_static("Zero", &RotationalInertia::Zero);
    rotational_inertia.def("__getitem__", [](const RotationalInertia &inertia, int i)
    {
        if (i < 0 || i > 8)
            throw py::index_error("RotationalInertia index out of range");

        return inertia.data[i];
    });
    rotational_inertia.def("__setitem__", [](RotationalInertia &inertia, int i, double value)
    {
        if (i < 0 || i > 8)
            throw py::index_error("RotationalInertia index out of range");

        inertia.data[i] = value;
    });
    rotational_inertia.def(double() * py::self);
    rotational_inertia.def(py::self + py::self);
    rotational_inertia.def(py::self * Vector());


    // --------------------
    // RigidBodyInertia
    // --------------------
    py::class_<RigidBodyInertia> rigid_body_inertia(m, "RigidBodyInertia");
    rigid_body_inertia.def(py::init<double, const Vector&, const RotationalInertia&>(),
                           py::arg("m")=0, py::arg_v("oc", Vector::Zero(), "Vector.Zero"),
                           py::arg_v("Ic", RotationalInertia::Zero(), "RigidBodyInertia.Zero"));
    rigid_body_inertia.def_static("Zero", &RigidBodyInertia::Zero);
    rigid_body_inertia.def("RefPoint", &RigidBodyInertia::RefPoint);
    rigid_body_inertia.def("getMass", &RigidBodyInertia::getMass);
    rigid_body_inertia.def("getCOG", &RigidBodyInertia::getCOG);
    rigid_body_inertia.def("getRotationalInertia", &RigidBodyInertia::getRotationalInertia);
    rigid_body_inertia.def(double() * py::self);
    rigid_body_inertia.def(py::self + py::self);
    rigid_body_inertia.def(py::self * Twist());
    rigid_body_inertia.def(Frame() * py::self);
    rigid_body_inertia.def(Rotation() * py::self);


    // --------------------
    // Segment
    // --------------------
    py::class_<Segment> segment(m, "Segment");
    segment.def(py::init<const std::string&, const Joint&, const Frame&, const RigidBodyInertia&>(),
                py::arg("name"), py::arg_v("joint", Joint(), "Joint"), py::arg_v("f_tip", Frame::Identity(), "Frame.Identity"),
                py::arg_v("I", RigidBodyInertia::Zero(), "RigidBodyInertia.Zero"));
    segment.def(py::init<const Joint&, const Frame&, const RigidBodyInertia&>(),
                py::arg_v("joint", Joint(), "Joint"), py::arg_v("f_tip", Frame::Identity(), "Frame.Identity"),
                py::arg_v("I", RigidBodyInertia::Zero(), "RigidBodyInertia.Zero"));
    segment.def(py::init<const Segment&>());
    segment.def("getFrameToTip", &Segment::getFrameToTip);
    segment.def("pose", &Segment::pose);
    segment.def("twist", &Segment::twist);
    segment.def("getName", &Segment::getName);
    segment.def("getJoint", &Segment::getJoint);
    segment.def("getInertia", &Segment::getInertia);
    segment.def("setInertia", &Segment::setInertia);


    // --------------------
    // Chain
    // --------------------
    py::class_<Chain> chain(m, "Chain");
    chain.def(py::init<>());
    chain.def(py::init<const Chain&>());
    chain.def("addSegment", &Chain::addSegment);
    chain.def("addChain", &Chain::addChain);
    chain.def("getNrOfJoints", &Chain::getNrOfJoints);
    chain.def("getNrOfSegments", &Chain::getNrOfSegments);
    chain.def("getSegment", (Segment& (Chain::*)(unsigned int)) &Chain::getSegment);
    chain.def("getSegment", (const Segment& (Chain::*)(unsigned int) const) &Chain::getSegment);
    chain.def("__repr__", [](const Chain &c)
    {
        std::ostringstream oss;
        oss << c;
        return oss.str();
    });


    // --------------------
    // Tree
    // --------------------
    py::class_<Tree> tree(m, "Tree");
    tree.def(py::init<const std::string&>(), py::arg("root_name")="root");
    tree.def("addSegment", &Tree::addSegment);
    tree.def("addChain", &Tree::addChain);
    tree.def("addTree", &Tree::addTree);
    tree.def("getNrOfJoints", &Tree::getNrOfJoints);
    tree.def("getNrOfSegments", &Tree::getNrOfSegments);
    tree.def("getChain", [](const Tree &tree, const std::string& chain_root, const std::string& chain_tip)
    {
        Chain* chain = new Chain();
        tree.getChain(chain_root, chain_tip, *chain);
        return chain;
    });
    tree.def("__repr__", [](const Tree &t)
    {
        std::ostringstream oss;
        oss << t;
        return oss.str();
    });


    // --------------------
    // Jacobian
    // --------------------
    py::class_<Jacobian> jacobian(m, "Jacobian");
    jacobian.def(py::init<>());
    jacobian.def(py::init<unsigned int>());
    jacobian.def(py::init<const Jacobian&>());
    jacobian.def("rows", &Jacobian::rows);
    jacobian.def("columns", &Jacobian::columns);
    jacobian.def("resize", &Jacobian::resize);
    jacobian.def("getColumn", &Jacobian::getColumn);
    jacobian.def("setColumn", &Jacobian::setColumn);
    jacobian.def("changeRefPoint", &Jacobian::changeRefPoint);
    jacobian.def("changeBase", &Jacobian::changeBase);
    jacobian.def("changeRefFrame", &Jacobian::changeRefFrame);
    jacobian.def("__getitem__", [](const Jacobian &jac, std::tuple<int, int> idx)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 5 || j < 0 || (unsigned int)j >= jac.columns())
            throw py::index_error("Jacobian index out of range");
        return jac((unsigned int)i, (unsigned int)j);
    });
    jacobian.def("__setitem__", [](Jacobian &jac, std::tuple<int, int> idx, double value)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 5 || j < 0 || (unsigned int)j >= jac.columns())
            throw py::index_error("Jacobian index out of range");

        jac((unsigned int)i, (unsigned int)j) = value;
    });
    jacobian.def("__repr__", [](const Jacobian &jac)
    {
        std::ostringstream oss;
        oss << jac;
        return oss.str();
    });

    m.def("SetToZero", (void (*)(Jacobian&)) &KDL::SetToZero);
    m.def("changeRefPoint", (void (*)(const Jacobian&, const Vector&, Jacobian&)) &KDL::changeRefPoint);
    m.def("changeBase", (void (*)(const Jacobian&, const Rotation&, Jacobian&)) &KDL::changeBase);
    m.def("SetToZero", (void (*)(const Jacobian&, const Frame&, Jacobian&)) &KDL::changeRefFrame);


    // --------------------
    // JntArray
    // --------------------
    py::class_<JntArray> jnt_array(m, "JntArray");
    jnt_array.def(py::init<>());
    jnt_array.def(py::init<unsigned int>());
    jnt_array.def(py::init<const JntArray&>());
    jnt_array.def("rows", &JntArray::rows);
    jnt_array.def("columns", &JntArray::columns);
    jnt_array.def("resize", &JntArray::resize);
    jnt_array.def("__getitem__", [](const JntArray &ja, int i)
    {
        if (i < 0 || (unsigned int)i >= ja.rows())
            throw py::index_error("JntArray index out of range");

        return ja(i);
    });
    jnt_array.def("__setitem__", [](JntArray &ja, int i, double value)
    {
        if (i < 0 || (unsigned int)i >= ja.rows())
            throw py::index_error("JntArray index out of range");

        ja(i) = value;
    });
    jnt_array.def("__repr__", [](const JntArray &ja)
    {
        std::ostringstream oss;
        oss << ja;
        return oss.str();
    });
    jnt_array.def(py::self == py::self);

    m.def("Add", (void (*)(const JntArray&, const JntArray&, JntArray&)) &KDL::Add);
    m.def("Subtract", (void (*)(const JntArray&, const JntArray&, JntArray&)) &KDL::Subtract);
    m.def("Multiply", (void (*)(const JntArray&, const double&, JntArray&)) &KDL::Multiply);
    m.def("Divide", (void (*)(const JntArray&, const double&, JntArray&)) &KDL::Divide);
    m.def("MultiplyJacobian", (void (*)(const Jacobian&, const JntArray&, Twist&)) &KDL::MultiplyJacobian);
    m.def("SetToZero", (void (*)(JntArray&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const JntArray&, const JntArray&, double)) &KDL::Equal,
          py::arg("src1"), py::arg("src2"), py::arg("eps")=epsilon);


    // --------------------
    // JntArrayVel
    // --------------------
    py::class_<JntArrayVel> jnt_array_vel(m, "JntArrayVel");
    jnt_array_vel.def_readwrite("q", &JntArrayVel::q);
    jnt_array_vel.def_readwrite("qdot", &JntArrayVel::qdot);
    jnt_array_vel.def(py::init<unsigned int>());
    jnt_array_vel.def(py::init<const JntArray&, const JntArray&>());
    jnt_array_vel.def(py::init<const JntArray&>());
    jnt_array_vel.def("resize", &JntArrayVel::resize);
    jnt_array_vel.def("value", &JntArrayVel::value);
    jnt_array_vel.def("deriv", &JntArrayVel::deriv);

    m.def("Add", (void (*)(const JntArrayVel&, const JntArrayVel&, JntArrayVel&)) &KDL::Add);
    m.def("Add", (void (*)(const JntArrayVel&, const JntArray&, JntArrayVel&)) &KDL::Add);
    m.def("Subtract", (void (*)(const JntArrayVel&, const JntArrayVel&, JntArrayVel&)) &KDL::Subtract);
    m.def("Subtract", (void (*)(const JntArrayVel&, const JntArray&, JntArrayVel&)) &KDL::Subtract);
    m.def("Multiply", (void (*)(const JntArrayVel&, const double&, JntArrayVel&)) &KDL::Multiply);
    m.def("Multiply", (void (*)(const JntArrayVel&, const doubleVel&, JntArrayVel&)) &KDL::Multiply);
    m.def("Divide", (void (*)(const JntArrayVel&, const double&, JntArrayVel&)) &KDL::Divide);
    m.def("Divide", (void (*)(const JntArrayVel&, const doubleVel&, JntArrayVel&)) &KDL::Divide);
    m.def("SetToZero", (void (*)(JntArrayVel&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const JntArrayVel&, const JntArrayVel&, double)) &KDL::Equal,
          py::arg("src1"), py::arg("src2"), py::arg("eps")=epsilon);


    // --------------------
    // SolverI
    // --------------------
    py::class_<SolverI> solver_i(m, "SolverI");
    solver_i.def("getError", &SolverI::getError);
    solver_i.def("strError", &SolverI::strError);
    solver_i.def("updateInternalDataStructures", &SolverI::updateInternalDataStructures);


    // --------------------
    // ChainFkSolverPos
    // --------------------
    py::class_<ChainFkSolverPos, SolverI> chain_fk_solver_pos(m, "ChainFkSolverPos");
    chain_fk_solver_pos.def("JntToCart", (int (ChainFkSolverPos::*)(const JntArray&, Frame&, int)) &ChainFkSolverPos::JntToCart,
                            py::arg("q_in"), py::arg("p_out"), py::arg("segmentNr")=-1);
    chain_fk_solver_pos.def("JntToCart", (int (ChainFkSolverPos::*)(const JntArray&, std::vector<Frame>&, int)) &ChainFkSolverPos::JntToCart,
                            py::arg("q_in"), py::arg("p_out"), py::arg("segmentNr")=-1);


    // --------------------
    // ChainFkSolverVel
    // --------------------
    py::class_<ChainFkSolverVel, SolverI> chain_fk_solver_vel(m, "ChainFkSolverVel");
    chain_fk_solver_vel.def("JntToCart", (int (ChainFkSolverVel::*)(const JntArrayVel&, FrameVel&, int)) &ChainFkSolverVel::JntToCart,
                            py::arg("q_in"), py::arg("p_out"), py::arg("segmentNr")=-1);
//    Argument by reference doesn't work for container types
//    chain_fk_solver_vel.def("JntToCart", (int (ChainFkSolverVel::*)(const JntArrayVel&, std::vector<FrameVel>&, int)) &ChainFkSolverVel::JntToCart,
//                            py::arg("q_in"), py::arg("p_out"), py::arg("segmentNr")=-1);


    // ------------------------------
    // ChainFkSolverPos_recursive
    // ------------------------------
    py::class_<ChainFkSolverPos_recursive, ChainFkSolverPos> chain_fk_solver_pos_recursive(m, "ChainFkSolverPos_recursive");
    chain_fk_solver_pos_recursive.def(py::init<const Chain&>());


    // ------------------------------
    // ChainFkSolverVel_recursive
    // ------------------------------
    py::class_<ChainFkSolverVel_recursive, ChainFkSolverVel> chain_fk_solver_vel_recursive(m, "ChainFkSolverVel_recursive");
    chain_fk_solver_vel_recursive.def(py::init<const Chain&>());


    // --------------------
    // ChainIkSolverPos
    // --------------------
    py::class_<ChainIkSolverPos, SolverI> chain_ik_solver_pos(m, "ChainIkSolverPos");
    chain_ik_solver_pos.def("CartToJnt", (int (ChainIkSolverPos::*)(const JntArray&, const Frame&, JntArray&)) &ChainIkSolverPos::CartToJnt,
                            py::arg("q_init"), py::arg("p_in"), py::arg("q_out"));


    // --------------------
    // ChainIkSolverVel
    // --------------------
    py::class_<ChainIkSolverVel, SolverI> chain_ik_solver_vel(m, "ChainIkSolverVel");
    chain_ik_solver_vel.def("CartToJnt", (int (ChainIkSolverVel::*)(const JntArray&, const Twist&, JntArray&)) &ChainIkSolverVel::CartToJnt,
                            py::arg("q_in"), py::arg("v_in"), py::arg("qdot_out"));
//    Argument by reference doesn't work for container types
//    chain_ik_solver_vel.def("CartToJnt", (int (ChainIkSolverVel::*)(const JntArray&, const FrameVel&, JntArrayVel&)) &ChainIkSolverVel::CartToJnt,
//                            py::arg("q_init"), py::arg("v_in"), py::arg("q_out"));


    // ----------------------
    // ChainIkSolverPos_NR
    // ----------------------
    py::class_<ChainIkSolverPos_NR, ChainIkSolverPos> chain_ik_solver_pos_NR(m, "ChainIkSolverPos_NR");
    chain_ik_solver_pos_NR.def(py::init<const Chain&, ChainFkSolverPos&, ChainIkSolverVel&, unsigned int, double>(),
                               py::arg("chain"), py::arg("fksolver"), py::arg("iksolver"),
                               py::arg("maxiter")=100, py::arg("eps")=epsilon);


    // ---------------------------
    // ChainIkSolverPos_NR_JL
    // ---------------------------
    py::class_<ChainIkSolverPos_NR_JL, ChainIkSolverPos> chain_ik_solver_pos_NR_JL(m, "ChainIkSolverPos_NR_JL");
    chain_ik_solver_pos_NR_JL.def(py::init<const Chain&, const JntArray&, const JntArray&, ChainFkSolverPos&,
                                  ChainIkSolverVel&, unsigned int, double>(),
                                  py::arg("chain"), py::arg("q_min"), py::arg("q_max"), py::arg("fksolver"),
                                  py::arg("iksolver"), py::arg("maxiter")=100, py::arg("eps")=epsilon);


    // -------------------------
    // ChainIkSolverVel_pinv
    // -------------------------
    py::class_<ChainIkSolverVel_pinv, ChainIkSolverVel> chain_ik_solver_vel_pinv(m, "ChainIkSolverVel_pinv");
    chain_ik_solver_vel_pinv.def(py::init<const Chain&, double, int>(),
                                 py::arg("chain"), py::arg("eps")=0.00001, py::arg("maxiter")=150);


    // -------------------------
    // ChainIkSolverVel_wdls
    // -------------------------
    py::class_<ChainIkSolverVel_wdls, ChainIkSolverVel> chain_ik_solver_vel_wdls(m, "ChainIkSolverVel_wdls");
    chain_ik_solver_vel_wdls.def(py::init<const Chain&, double, int>(),
                                 py::arg("chain"), py::arg("eps")=0.00001, py::arg("maxiter")=150);
    chain_ik_solver_vel_wdls.def("setWeightTS", &ChainIkSolverVel_wdls::setWeightTS);
    chain_ik_solver_vel_wdls.def("setWeightJS", &ChainIkSolverVel_wdls::setWeightJS);
    chain_ik_solver_vel_wdls.def("setLambda", &ChainIkSolverVel_wdls::setLambda);
    chain_ik_solver_vel_wdls.def("setEps", &ChainIkSolverVel_wdls::setEps);
    chain_ik_solver_vel_wdls.def("setMaxIter", &ChainIkSolverVel_wdls::setMaxIter);
    chain_ik_solver_vel_wdls.def("getNrZeroSigmas", &ChainIkSolverVel_wdls::getNrZeroSigmas);
    chain_ik_solver_vel_wdls.def("getSigmaMin", &ChainIkSolverVel_wdls::getSigmaMin);
    chain_ik_solver_vel_wdls.def("getSigma", &ChainIkSolverVel_wdls::getSigma);
    chain_ik_solver_vel_wdls.def("getEps", &ChainIkSolverVel_wdls::getEps);
    chain_ik_solver_vel_wdls.def("getLambda", &ChainIkSolverVel_wdls::getLambda);
    chain_ik_solver_vel_wdls.def("getLambdaScaled", &ChainIkSolverVel_wdls::getLambdaScaled);
    chain_ik_solver_vel_wdls.def("getSVDResult", &ChainIkSolverVel_wdls::getSVDResult);


    // -------------------------
    // ChainIkSolverPos_LMA
    // -------------------------
    py::class_<ChainIkSolverPos_LMA, ChainIkSolverPos> chain_ik_solver_pos_LMA(m, "ChainIkSolverPos_LMA");
    chain_ik_solver_pos_LMA.def(py::init<const Chain&, double, int, double>(),
                                py::arg("chain"), py::arg("eps")=1e-5, py::arg("maxiter")=500,
                                py::arg("eps_joints")=1e-15);


    // ----------------------------
    // ChainIkSolverVel_pinv_nso
    // ----------------------------
    py::class_<ChainIkSolverVel_pinv_nso, ChainIkSolverVel> chain_ik_solver_vel_pinv_nso(m, "ChainIkSolverVel_pinv_nso");
    chain_ik_solver_vel_pinv_nso.def(py::init<const Chain&, double, int, double>(),
                                     py::arg("chain"), py::arg("eps")=0.00001, py::arg("maxiter")=150, py::arg("alpha")=0.25);
    chain_ik_solver_vel_pinv_nso.def("setWeights", &ChainIkSolverVel_pinv_nso::setWeights);
    chain_ik_solver_vel_pinv_nso.def("setOptPos", &ChainIkSolverVel_pinv_nso::setOptPos);
    chain_ik_solver_vel_pinv_nso.def("setAlpha", &ChainIkSolverVel_pinv_nso::setAlpha);
    chain_ik_solver_vel_pinv_nso.def("getWeights", &ChainIkSolverVel_pinv_nso::getWeights);
    chain_ik_solver_vel_pinv_nso.def("getOptPos", &ChainIkSolverVel_pinv_nso::getOptPos);
    chain_ik_solver_vel_pinv_nso.def("getAlpha", &ChainIkSolverVel_pinv_nso::getAlpha);


    // -------------------------------
    // ChainIkSolverVel_pinv_givens
    // -------------------------------
    py::class_<ChainIkSolverVel_pinv_givens, ChainIkSolverVel> chain_ik_solver_vel_pinv_givens(m, "ChainIkSolverVel_pinv_givens");
    chain_ik_solver_vel_pinv_givens.def(py::init<const Chain&>());


    // ------------------------------
    // ChainJntToJacSolver
    // ------------------------------
    py::class_<ChainJntToJacSolver, SolverI> chain_jnt_to_jac_solver(m, "ChainJntToJacSolver");
    chain_jnt_to_jac_solver.def(py::init<const Chain&>());
    chain_jnt_to_jac_solver.def("JntToJac", &ChainJntToJacSolver::JntToJac,
                                py::arg("q_in"), py::arg("jac"), py::arg("seg_nr")=-1);
    chain_jnt_to_jac_solver.def("setLockedJoints", &ChainJntToJacSolver::setLockedJoints);


    // ------------------------------
    // ChainJntToJacDotSolver
    // ------------------------------
    py::class_<ChainJntToJacDotSolver, SolverI> chain_jnt_to_jac_dot_solver(m, "ChainJntToJacDotSolver");
    chain_jnt_to_jac_dot_solver.def(py::init<const Chain&>());
    chain_jnt_to_jac_dot_solver.def("JntToJacDot", (int (ChainJntToJacDotSolver::*)(const JntArrayVel&, Jacobian&, int)) &ChainJntToJacDotSolver::JntToJacDot,
                                    py::arg("q_in"), py::arg("jdot"), py::arg("seg_nr")=-1);
    chain_jnt_to_jac_dot_solver.def("JntToJacDot", (int (ChainJntToJacDotSolver::*)(const JntArrayVel&, Twist&, int)) &ChainJntToJacDotSolver::JntToJacDot,
                                    py::arg("q_in"), py::arg("jac_dot_q_dot"), py::arg("seg_nr")=-1);
    chain_jnt_to_jac_dot_solver.def("setLockedJoints", &ChainJntToJacDotSolver::setLockedJoints,
                                    py::arg("locked_joints"));

    chain_jnt_to_jac_dot_solver.def_readonly_static("E_JAC_DOT_FAILED", &ChainJntToJacDotSolver::E_JAC_DOT_FAILED);
    chain_jnt_to_jac_dot_solver.def_readonly_static("E_JACSOLVER_FAILED", &ChainJntToJacDotSolver::E_JACSOLVER_FAILED);
    chain_jnt_to_jac_dot_solver.def_readonly_static("E_FKSOLVERPOS_FAILED", &ChainJntToJacDotSolver::E_FKSOLVERPOS_FAILED);

    chain_jnt_to_jac_dot_solver.def_readonly_static("HYBRID", &ChainJntToJacDotSolver::HYBRID);
    chain_jnt_to_jac_dot_solver.def_readonly_static("BODYFIXED", &ChainJntToJacDotSolver::BODYFIXED);
    chain_jnt_to_jac_dot_solver.def_readonly_static("INERTIAL", &ChainJntToJacDotSolver::INERTIAL);

    chain_jnt_to_jac_dot_solver.def("setHybridRepresentation", &ChainJntToJacDotSolver::setHybridRepresentation);
    chain_jnt_to_jac_dot_solver.def("setBodyFixedRepresentation", &ChainJntToJacDotSolver::setBodyFixedRepresentation);
    chain_jnt_to_jac_dot_solver.def("setInertialRepresentation", &ChainJntToJacDotSolver::setInertialRepresentation);
    chain_jnt_to_jac_dot_solver.def("setRepresentation", &ChainJntToJacDotSolver::setRepresentation,
                                    py::arg("representation"));


    // ------------------------------
    // ChainIdSolver
    // ------------------------------
    py::class_<ChainIdSolver, SolverI> chain_id_solver(m, "ChainIdSolver");
    chain_id_solver.def("CartToJnt", &ChainIdSolver::CartToJnt);


    // ------------------------------
    // ChainIdSolver_RNE
    // ------------------------------
    py::class_<ChainIdSolver_RNE, ChainIdSolver> chain_id_solver_RNE(m, "ChainIdSolver_RNE");
    chain_id_solver_RNE.def(py::init<const Chain&, Vector>());
}
