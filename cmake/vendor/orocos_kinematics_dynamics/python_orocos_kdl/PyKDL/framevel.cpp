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


#include <kdl/framevel.hpp>
#include <kdl/framevel_io.hpp>
#include "PyKDL.h"

namespace py = pybind11;
using namespace KDL;


void init_framevel(pybind11::module &m)
{
    // --------------------
    // doubleVel
    // --------------------
    py::class_<doubleVel > double_vel(m, "doubleVel");
    double_vel.def(py::init<>());
    double_vel.def(py::init<const double>());
    double_vel.def(py::init<const double, const double>());
    double_vel.def(py::init<const doubleVel&>());
    double_vel.def_readwrite("t", &doubleVel::t);
    double_vel.def_readwrite("grad", &doubleVel::grad);
    double_vel.def("value", &doubleVel::value);
    double_vel.def("deriv", &doubleVel::deriv);
    double_vel.def("__repr__", [](const doubleVel &d)
    {
        std::ostringstream oss;
        oss << d;
        return oss.str();
    });
    double_vel.def("__copy__", [](const doubleVel& self)
    {
        return doubleVel(self);
    });
    double_vel.def("__deepcopy__", [](const doubleVel& self, py::dict)
    {
        return doubleVel(self);
    }, py::arg("memo"));

    double_vel.def(py::self == py::self);
    double_vel.def(py::self != py::self);

    double_vel.def("__neg__", [](const doubleVel &a)
    {
        return operator-(a);
    }, py::is_operator());

    m.def("diff", (doubleVel (*)(const doubleVel&, const doubleVel&, double)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt")=1.0);
    m.def("addDelta", (doubleVel (*)(const doubleVel&, const doubleVel&, double)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt")=1.0);
    m.def("Equal", (bool (*)(const doubleVel&, const doubleVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);


    // --------------------
    // VectorVel
    // --------------------
    py::class_<VectorVel> vector_vel(m, "VectorVel");
    vector_vel.def_readwrite("p", &VectorVel::p);
    vector_vel.def_readwrite("v", &VectorVel::v);
    vector_vel.def(py::init<>());
    vector_vel.def(py::init<const Vector&, const Vector&>());
    vector_vel.def(py::init<const Vector&>());
    vector_vel.def(py::init<const VectorVel&>());
    vector_vel.def("value", &VectorVel::value);
    vector_vel.def("deriv", &VectorVel::deriv);
    vector_vel.def("__repr__", [](const VectorVel &vv)
    {
        std::ostringstream oss;
        oss << vv;
        return oss.str();
    });
    vector_vel.def("__copy__", [](const VectorVel& self)
    {
        return VectorVel(self);
    });
    vector_vel.def("__deepcopy__", [](const VectorVel& self, py::dict)
    {
        return VectorVel(self);
    }, py::arg("memo"));
    vector_vel.def_static("Zero", &VectorVel::Zero);
    vector_vel.def("ReverseSign", &VectorVel::ReverseSign);
    vector_vel.def("Norm", &VectorVel::Norm, py::arg("eps")=epsilon);
    vector_vel.def(py::self += py::self);
    vector_vel.def(py::self -= py::self);
    vector_vel.def(py::self + py::self);
    vector_vel.def(py::self - py::self);
    vector_vel.def(Vector() + py::self);
    vector_vel.def(Vector() - py::self);
    vector_vel.def(py::self + Vector());
    vector_vel.def(py::self - Vector());

    vector_vel.def(py::self * py::self);
    vector_vel.def(py::self * Vector());
    vector_vel.def(Vector() * py::self);
    vector_vel.def(double() * py::self);
    vector_vel.def(py::self * double());
    vector_vel.def(doubleVel() * py::self);
    vector_vel.def(py::self * doubleVel());
    vector_vel.def(Rotation() * py::self);

    vector_vel.def(py::self / double());
    vector_vel.def(py::self / doubleVel());

    vector_vel.def(py::self == py::self);
    vector_vel.def(py::self != py::self);
    vector_vel.def(Vector() == py::self);
    vector_vel.def(Vector() != py::self);
    vector_vel.def(py::self == Vector());
    vector_vel.def(py::self != Vector());
    vector_vel.def("__neg__", [](const VectorVel &a)
    {
        return operator-(a);
    }, py::is_operator());
    vector_vel.def(py::pickle(
            [](const VectorVel &vv)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(vv.p, vv.v);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                VectorVel vv(t[0].cast<Vector>(), t[1].cast<Vector>());
                return vv;
            }));

    m.def("SetToZero", (void (*)(VectorVel&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const VectorVel&, const VectorVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const Vector&, const VectorVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const VectorVel&, const Vector&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);

    m.def("dot", (doubleVel (*)(const VectorVel&, const VectorVel&)) &KDL::dot);
    m.def("dot", (doubleVel (*)(const VectorVel&, const Vector&)) &KDL::dot);
    m.def("dot", (doubleVel (*)(const Vector&, const VectorVel&)) &KDL::dot);


    // --------------------
    // TwistVel
    // --------------------
    py::class_<TwistVel> twist_vel(m, "TwistVel");
    twist_vel.def_readwrite("vel", &TwistVel::vel);
    twist_vel.def_readwrite("rot", &TwistVel::rot);
    twist_vel.def(py::init<>());
    twist_vel.def(py::init<const VectorVel&, const VectorVel&>());
    twist_vel.def(py::init<const Twist&, const Twist&>());
    twist_vel.def(py::init<const Twist&>());
    twist_vel.def(py::init<const TwistVel&>());
    twist_vel.def("value", &TwistVel::value);
    twist_vel.def("deriv", &TwistVel::deriv);
    twist_vel.def("__repr__", [](const TwistVel &tv)
    {
        std::ostringstream oss;
        oss << tv;
        return oss.str();
    });
    twist_vel.def("__copy__", [](const TwistVel& self)
    {
        return TwistVel(self);
    });
    twist_vel.def("__deepcopy__", [](const TwistVel& self, py::dict)
    {
        return TwistVel(self);
    }, py::arg("memo"));
    twist_vel.def_static("Zero", &TwistVel::Zero);
    twist_vel.def("ReverseSign", &TwistVel::ReverseSign);
    twist_vel.def("RefPoint", &TwistVel::RefPoint);
    twist_vel.def("GetTwist", &TwistVel::GetTwist);
    twist_vel.def("GetTwistDot", &TwistVel::GetTwistDot);

    twist_vel.def(py::self -= py::self);
    twist_vel.def(py::self += py::self);
    twist_vel.def(py::self * double());
    twist_vel.def(double() * py::self);
    twist_vel.def(py::self / double());

    twist_vel.def(py::self * doubleVel());
    twist_vel.def(doubleVel() * py::self);
    twist_vel.def(py::self / doubleVel());

    twist_vel.def(py::self + py::self);
    twist_vel.def(py::self - py::self);

    twist_vel.def(py::self == py::self);
    twist_vel.def(py::self != py::self);
    twist_vel.def(Twist() == py::self);
    twist_vel.def(Twist() != py::self);
    twist_vel.def(py::self == Twist());
    twist_vel.def(py::self != Twist());
    twist_vel.def("__neg__", [](const TwistVel &a)
    {
        return operator-(a);
    }, py::is_operator());
    twist_vel.def(py::pickle(
            [](const TwistVel &tv)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(tv.vel, tv.rot);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                TwistVel tv(t[0].cast<VectorVel>(), t[1].cast<VectorVel>());
                return tv;
            }));

    m.def("SetToZero", (void (*)(TwistVel&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const TwistVel&, const TwistVel&, double)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const Twist&, const TwistVel&, double)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const TwistVel&, const Twist&, double)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // RotationVel
    // --------------------
    py::class_<RotationVel> rotation_vel(m, "RotationVel");
    rotation_vel.def_readwrite("R", &RotationVel::R);
    rotation_vel.def_readwrite("w", &RotationVel::w);
    rotation_vel.def(py::init<>());
    rotation_vel.def(py::init<const Rotation&>());
    rotation_vel.def(py::init<const Rotation&, const Vector&>());
    rotation_vel.def(py::init<const RotationVel&>());
    rotation_vel.def("value", &RotationVel::value);
    rotation_vel.def("deriv", &RotationVel::deriv);
    rotation_vel.def("__repr__", [](const RotationVel &rv)
    {
        std::ostringstream oss;
        oss << rv;
        return oss.str();
    });
    rotation_vel.def("__copy__", [](const RotationVel& self)
    {
        return RotationVel(self);
    });
    rotation_vel.def("__deepcopy__", [](const RotationVel& self, py::dict)
    {
        return RotationVel(self);
    }, py::arg("memo"));
    rotation_vel.def("UnitX", &RotationVel::UnitX);
    rotation_vel.def("UnitY", &RotationVel::UnitY);
    rotation_vel.def("UnitZ", &RotationVel::UnitZ);
    rotation_vel.def_static("Identity", &RotationVel::Identity);
    rotation_vel.def("Inverse", (RotationVel (RotationVel::*)(void) const) &RotationVel::Inverse);
    rotation_vel.def("Inverse", (VectorVel (RotationVel::*)(const VectorVel&) const) &RotationVel::Inverse);
    rotation_vel.def("Inverse", (VectorVel (RotationVel::*)(const Vector&) const) &RotationVel::Inverse);
    rotation_vel.def("DoRotX", &RotationVel::DoRotX);
    rotation_vel.def("DoRotY", &RotationVel::DoRotY);
    rotation_vel.def("DoRotZ", &RotationVel::DoRotZ);
    rotation_vel.def_static("RotX", &RotationVel::RotX);
    rotation_vel.def_static("RotY", &RotationVel::RotY);
    rotation_vel.def_static("RotZ", &RotationVel::RotZ);
    rotation_vel.def_static("Rot", &RotationVel::Rot);
    rotation_vel.def_static("Rot2", &RotationVel::Rot2);

    rotation_vel.def("Inverse", (TwistVel (RotationVel::*)(const TwistVel&) const) &RotationVel::Inverse);
    rotation_vel.def("Inverse", (TwistVel (RotationVel::*)(const Twist&) const) &RotationVel::Inverse);

    rotation_vel.def(py::self * VectorVel());
    rotation_vel.def(py::self * Vector());
    rotation_vel.def(py::self * TwistVel());
    rotation_vel.def(py::self * Twist());
    rotation_vel.def(py::self * py::self);
    rotation_vel.def(Rotation() * py::self);
    rotation_vel.def(py::self * Rotation());

    rotation_vel.def(py::self == py::self);
    rotation_vel.def(py::self != py::self);
    rotation_vel.def(Rotation() == py::self);
    rotation_vel.def(Rotation() != py::self);
    rotation_vel.def(py::self == Rotation());
    rotation_vel.def(py::self != Rotation());
    rotation_vel.def(py::pickle(
            [](const RotationVel &rv)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(rv.R, rv.w);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                RotationVel rv(t[0].cast<Rotation>(), t[1].cast<Vector>());
                return rv;
            }));

    m.def("Equal", (bool (*)(const RotationVel&, const RotationVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const Rotation&, const RotationVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const RotationVel&, const Rotation&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);


    // --------------------
    // FrameVel
    // --------------------
    py::class_<FrameVel> frame_vel(m, "FrameVel");
    frame_vel.def_readwrite("M", &FrameVel::M);
    frame_vel.def_readwrite("p", &FrameVel::p);
    frame_vel.def(py::init<>());
    frame_vel.def(py::init<const Frame&>());
    frame_vel.def(py::init<const Frame&, const Twist&>());
    frame_vel.def(py::init<const RotationVel&, const VectorVel&>());
    frame_vel.def(py::init<const FrameVel&>());
    frame_vel.def("value", &FrameVel::value);
    frame_vel.def("deriv", &FrameVel::deriv);
    frame_vel.def("__repr__", [](const FrameVel &fv)
    {
        std::ostringstream oss;
        oss << fv;
        return oss.str();
    });
    frame_vel.def("__copy__", [](const FrameVel& self)
    {
        return FrameVel(self);
    });
    frame_vel.def("__deepcopy__", [](const FrameVel& self, py::dict)
    {
        return FrameVel(self);
    }, py::arg("memo"));
    frame_vel.def_static("Identity", &FrameVel::Identity);
    frame_vel.def("Inverse", (FrameVel (FrameVel::*)() const) &FrameVel::Inverse);
    frame_vel.def("Inverse", (VectorVel (FrameVel::*)(const VectorVel&) const) &FrameVel::Inverse);
    frame_vel.def("Inverse", (VectorVel (FrameVel::*)(const Vector&) const) &FrameVel::Inverse);
    frame_vel.def(py::self * VectorVel());
    frame_vel.def(py::self * Vector());
    frame_vel.def("GetFrame", &FrameVel::GetFrame);
    frame_vel.def("GetTwist", &FrameVel::GetTwist);
    frame_vel.def("Inverse", (TwistVel (FrameVel::*)(const TwistVel&) const) &FrameVel::Inverse);
    frame_vel.def("Inverse", (TwistVel (FrameVel::*)(const Twist&) const) &FrameVel::Inverse);
    frame_vel.def(py::self * TwistVel());
    frame_vel.def(py::self * Twist());
    frame_vel.def(py::self * py::self);
    frame_vel.def(Frame() * py::self);
    frame_vel.def(py::self * Frame());

    frame_vel.def(py::self == py::self);
    frame_vel.def(py::self != py::self);
    frame_vel.def(Frame() == py::self);
    frame_vel.def(Frame() != py::self);
    frame_vel.def(py::self == Frame());
    frame_vel.def(py::self != Frame());
    frame_vel.def(py::pickle(
            [](const FrameVel &fv)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(fv.M, fv.p);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                FrameVel rv(t[0].cast<RotationVel>(), t[1].cast<VectorVel>());
                return rv;
            }));

    m.def("Equal", (bool (*)(const FrameVel&, const FrameVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const Frame&, const FrameVel&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);
    m.def("Equal", (bool (*)(const FrameVel&, const Frame&, double)) &KDL::Equal,
          py::arg("r1"), py::arg("r2"), py::arg("eps")=epsilon);


    // --------------------
    // Global
    // --------------------
    m.def("diff", (VectorVel (*)(const VectorVel&, const VectorVel&, double)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt")=1.0);
    m.def("diff", (VectorVel (*)(const RotationVel&, const RotationVel&, double)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt")=1.0);
    m.def("diff", (TwistVel (*)(const FrameVel&, const FrameVel&, double)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt")=1.0);

    m.def("addDelta", (VectorVel (*)(const VectorVel&, const VectorVel&, double)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt")=1.0);
    m.def("addDelta", (RotationVel (*)(const RotationVel&, const VectorVel&, double)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt")=1.0);
    m.def("addDelta", (FrameVel (*)(const FrameVel&, const TwistVel&, double)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt")=1.0);
}
