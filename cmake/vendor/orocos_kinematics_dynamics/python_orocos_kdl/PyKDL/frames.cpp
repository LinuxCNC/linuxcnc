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


#include <kdl/frames.hpp>
#include <kdl/frames_io.hpp>
#include "PyKDL.h"

namespace py = pybind11;
using namespace KDL;


void init_frames(py::module &m)
{
    // --------------------
    // Vector
    // --------------------
    py::class_<Vector> vector(m, "Vector");
    vector.def(py::init<>());
    vector.def(py::init<double, double, double>());
    vector.def(py::init<const Vector&>());
    vector.def("x", (void (Vector::*)(double)) &Vector::x);
    vector.def("y", (void (Vector::*)(double)) &Vector::y);
    vector.def("z", (void (Vector::*)(double)) &Vector::z);
    vector.def("x", (double (Vector::*)(void) const) &Vector::x);
    vector.def("y", (double (Vector::*)(void) const) &Vector::y);
    vector.def("z", (double (Vector::*)(void) const) &Vector::z);
    vector.def("__getitem__", [](const Vector &v, int i)
    {
        if (i < 0 || i > 2)
            throw py::index_error("Vector index out of range");

        return v(i);
    });
    vector.def("__setitem__", [](Vector &v, int i, double value)
    {
        if (i < 0 || i > 2)
            throw py::index_error("Vector index out of range");

        v(i) = value;
    });
    vector.def("__repr__", [](const Vector &v)
    {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    });
    vector.def("ReverseSign", &Vector::ReverseSign);
    vector.def(py::self -= py::self);
    vector.def(py::self += py::self);
    vector.def(py::self + py::self);
    vector.def(py::self - py::self);
    vector.def(py::self * double());
    vector.def(double() * py::self);
    vector.def(py::self / double());
    vector.def(py::self * py::self);
    vector.def(py::self == py::self);
    vector.def(py::self != py::self);
    vector.def("__neg__", [](const Vector &a)
    {
        return operator-(a);
    }, py::is_operator());
    vector.def("__copy__", [](const Vector& self)
    {
        return Vector(self);
    });
    vector.def("__deepcopy__", [](const Vector& self, py::dict)
    {
        return Vector(self);
    }, py::arg("memo"));
    vector.def_static("Zero", &Vector::Zero);
    vector.def("Norm", &Vector::Norm, py::arg("eps")=epsilon);
    vector.def("Normalize", &Vector::Normalize, py::arg("eps")=epsilon);
    vector.def(py::pickle(
            [](const Vector &v)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(v.x(), v.y(), v.z());
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 3)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                Vector v(t[0].cast<double>(), t[1].cast<double>(), t[2].cast<double>());
                return v;
            }));

    m.def("SetToZero", (void (*)(Vector&)) &KDL::SetToZero);
    m.def("dot", (double (*)(const Vector&, const Vector&)) &KDL::dot);
    m.def("Equal", (bool (*)(const Vector&, const Vector&, double)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // Wrench
    // --------------------
    py::class_<Wrench> wrench(m, "Wrench");
    wrench.def(py::init<>());
    wrench.def(py::init<const Vector&, const Vector&>());
    wrench.def(py::init<const Wrench&>());
    wrench.def_readwrite("force", &Wrench::force);
    wrench.def_readwrite("torque", &Wrench::torque);
    wrench.def("__getitem__", [](const Wrench &t, int i)
    {
        if (i < 0 || i > 5)
            throw py::index_error("Wrench index out of range");

        return t(i);
    });
    wrench.def("__setitem__", [](Wrench &t, int i, double value)
    {
        if (i < 0 || i > 5)
            throw py::index_error("Wrench index out of range");

        t(i) = value;
    });
    wrench.def("__repr__", [](const Wrench &t)
    {
        std::ostringstream oss;
        oss << t;
        return oss.str();
    });
    wrench.def("__copy__", [](const Wrench& self)
    {
        return Wrench(self);
    });
    wrench.def("__deepcopy__", [](const Wrench& self, py::dict)
    {
        return Wrench(self);
    }, py::arg("memo"));
    wrench.def_static("Zero", &Wrench::Zero);
    wrench.def("ReverseSign", &Wrench::ReverseSign);
    wrench.def("RefPoint", &Wrench::RefPoint);
    wrench.def(py::self -= py::self);
    wrench.def(py::self += py::self);
    wrench.def(py::self * double());
    wrench.def(double() * py::self);
    wrench.def(py::self / double());
    wrench.def(py::self + py::self);
    wrench.def(py::self - py::self);
    wrench.def(py::self == py::self);
    wrench.def(py::self != py::self);
    wrench.def("__neg__", [](const Wrench &w)
    {
        return operator-(w);
    }, py::is_operator());
    wrench.def(py::pickle(
            [](const Wrench &wr)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(wr.force, wr.torque);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                Wrench wr(t[0].cast<Vector>(), t[1].cast<Vector>());
                return wr;
            }));

    m.def("SetToZero", (void (*)(Wrench&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const Wrench&, const Wrench&, double eps)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // Twist
    // --------------------
    py::class_<Twist> twist(m, "Twist");
    twist.def(py::init<>());
    twist.def(py::init<const Vector&, const Vector&>());
    twist.def(py::init<const Twist&>());
    twist.def_readwrite("vel", &Twist::vel);
    twist.def_readwrite("rot", &Twist::rot);
    twist.def("__getitem__", [](const Twist &t, int i)
    {
        if (i < 0 || i > 5)
            throw py::index_error("Twist index out of range");

        return t(i);
    });
    twist.def("__setitem__", [](Twist &t, int i, double value)
    {
        if (i < 0 || i > 5)
            throw py::index_error("Twist index out of range");

        t(i) = value;
    });
    twist.def("__repr__", [](const Twist &t)
    {
        std::ostringstream oss;
        oss << t;
        return oss.str();
    });
    twist.def("__copy__", [](const Twist& self)
    {
        return Twist(self);
    });
    twist.def("__deepcopy__", [](const Twist& self, py::dict)
    {
        return Twist(self);
    }, py::arg("memo"));
    twist.def_static("Zero", &Twist::Zero);
    twist.def("ReverseSign", &Twist::ReverseSign);
    twist.def("RefPoint", &Twist::RefPoint);
    twist.def(py::self -= py::self);
    twist.def(py::self += py::self);
    twist.def(py::self * double());
    twist.def(double() * py::self);
    twist.def(py::self / double());
    twist.def(py::self + py::self);
    twist.def(py::self - py::self);
    twist.def(py::self == py::self);
    twist.def(py::self != py::self);
    twist.def("__neg__", [](const Twist &a)
    {
        return operator-(a);
    }, py::is_operator());
    twist.def(py::pickle(
            [](const Twist &tt)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(tt.vel, tt.rot);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                Twist tt(t[0].cast<Vector>(), t[1].cast<Vector>());
                return tt;
            }));

    m.def("dot", (double (*)(const Twist&, const Wrench&)) &KDL::dot);
    m.def("dot", (double (*)(const Wrench&, const Twist&)) &KDL::dot);
    m.def("SetToZero", (void (*)(Twist&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const Twist&, const Twist&, double eps)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // Rotation
    // --------------------
    py::class_<Rotation> rotation(m, "Rotation");
    rotation.def(py::init<>());
    rotation.def(py::init<double, double, double, double, double, double, double, double, double>());
    rotation.def(py::init<const Vector&, const Vector&, const Vector&>());
    rotation.def(py::init<const Rotation&>());
    rotation.def("__getitem__", [](const Rotation &r, std::tuple<int, int> idx)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 2 || j < 0 || j > 2)
            throw py::index_error("Rotation index out of range");

        return r(i, j);
    });
    rotation.def("__setitem__", [](Rotation &r, std::tuple<int, int> idx, double value)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 2 || j < 0 || j > 2)
            throw py::index_error("Rotation index out of range");

        r(i, j) = value;
    });
    rotation.def("__repr__", [](const Rotation &r)
    {
            std::ostringstream oss;
            oss << r;
            return oss.str();
    });
    rotation.def("__copy__", [](const Rotation& self)
    {
        return Rotation(self);
    });
    rotation.def("__deepcopy__", [](const Rotation& self, py::dict)
    {
        return Rotation(self);
    }, py::arg("memo"));
    rotation.def("SetInverse", &Rotation::SetInverse);
    rotation.def("Inverse", (Rotation (Rotation::*)(void) const) &Rotation::Inverse);
    rotation.def("Inverse", (Vector (Rotation::*)(const Vector&) const) &Rotation::Inverse);
    rotation.def("Inverse", (Wrench (Rotation::*)(const Wrench&) const) &Rotation::Inverse);
    rotation.def("Inverse", (Twist (Rotation::*)(const Twist&) const) &Rotation::Inverse);
    rotation.def_static("Identity", &Rotation::Identity);
    rotation.def_static("RotX", &Rotation::RotX);
    rotation.def_static("RotY", &Rotation::RotY);
    rotation.def_static("RotZ", &Rotation::RotZ);
    rotation.def_static("Rot", &Rotation::Rot);
    rotation.def_static("Rot2", &Rotation::Rot2);
    rotation.def_static("EulerZYZ", &Rotation::EulerZYZ);
    rotation.def_static("RPY", &Rotation::RPY);
    rotation.def_static("EulerZYX", &Rotation::EulerZYX);
    rotation.def_static("Quaternion", &Rotation::Quaternion);
    rotation.def("DoRotX", &Rotation::DoRotX);
    rotation.def("DoRotY", &Rotation::DoRotY);
    rotation.def("DoRotZ", &Rotation::DoRotZ);
    rotation.def("GetRot", &Rotation::GetRot);
    rotation.def("GetRotAngle", [](const Rotation &r, double eps)
    {
        Vector axis;
        double ret = r.GetRotAngle(axis, eps);
        return py::make_tuple(ret, axis);
    }, py::arg("eps") = epsilon);
    rotation.def("GetEulerZYZ", [](const Rotation &r)
    {
        double Alfa, Beta, Gamma;
        r.GetEulerZYZ(Alfa, Beta, Gamma);
        return py::make_tuple(Alfa, Beta, Gamma);
    });
    rotation.def("GetRPY", [](const Rotation &r)
    {
        double roll, pitch, yaw;
        r.GetRPY(roll, pitch, yaw);
        return py::make_tuple(roll, pitch, yaw);
    });
    rotation.def("GetEulerZYX", [](const Rotation &r)
    {
        double Alfa, Beta, Gamma;
        r.GetEulerZYX(Alfa, Beta, Gamma);
        return py::make_tuple(Alfa, Beta, Gamma);
    });
    rotation.def("GetQuaternion", [](const Rotation &r)
    {
        double x, y, z, w;
        r.GetQuaternion(x, y, z, w);
        return py::make_tuple(x, y, z, w);
    });
    rotation.def("UnitX", (Vector (Rotation::*)() const) &Rotation::UnitX);
    rotation.def("UnitY", (Vector (Rotation::*)() const) &Rotation::UnitY);
    rotation.def("UnitZ", (Vector (Rotation::*)() const) &Rotation::UnitZ);
    rotation.def("UnitX", (void (Rotation::*)(const Vector&)) &Rotation::UnitX);
    rotation.def("UnitY", (void (Rotation::*)(const Vector&)) &Rotation::UnitY);
    rotation.def("UnitZ", (void (Rotation::*)(const Vector&)) &Rotation::UnitZ);
    rotation.def(py::self * Vector());
    rotation.def(py::self * Twist());
    rotation.def(py::self * Wrench());
    rotation.def(py::self == py::self);
    rotation.def(py::self != py::self);
    rotation.def(py::self * py::self);
    rotation.def(py::pickle(
            [](const Rotation &rot)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                double roll{0}, pitch{0}, yaw{0};
                rot.GetRPY(roll, pitch, yaw);
                return py::make_tuple(roll, pitch, yaw);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 3)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                return Rotation::RPY(t[0].cast<double>(), t[1].cast<double>(), t[2].cast<double>());
            }));

    m.def("Equal", (bool (*)(const Rotation&, const Rotation&, double eps)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // Frame
    // --------------------
    py::class_<Frame> frame(m, "Frame");
    frame.def(py::init<const Rotation&, const Vector&>());
    frame.def(py::init<const Vector&>());
    frame.def(py::init<const Rotation&>());
    frame.def(py::init<const Frame&>());
    frame.def(py::init<>());
    frame.def_readwrite("M", &Frame::M);
    frame.def_readwrite("p", &Frame::p);
    frame.def("__getitem__", [](const Frame &frm, std::tuple<int, int> idx)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 2 || j < 0 || j > 3)
            throw py::index_error("Frame index out of range");

        return frm(i, j);
    });
    frame.def("__setitem__", [](Frame &frm, std::tuple<int, int> idx, double value)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || i > 2 || j < 0 || j > 3)
            throw py::index_error("Frame index out of range");

        if (j == 3)
            frm.p(i) = value;
        else
            frm.M(i, j) = value;
    });
    frame.def("__repr__", [](const Frame &frm)
    {
        std::ostringstream oss;
        oss << frm;
        return oss.str();
    });
    frame.def("__copy__", [](const Frame& self)
    {
        return Frame(self);
    });
    frame.def("__deepcopy__", [](const Frame& self, py::dict)
    {
        return Frame(self);
    }, py::arg("memo"));
    frame.def("DH_Craig1989", &Frame::DH_Craig1989);
    frame.def("DH", &Frame::DH);
    frame.def("Inverse", (Frame (Frame::*)() const) &Frame::Inverse);
    frame.def("Inverse", (Vector (Frame::*)(const Vector&) const) &Frame::Inverse);
    frame.def("Inverse", (Wrench (Frame::*)(const Wrench&) const) &Frame::Inverse);
    frame.def("Inverse", (Twist (Frame::*)(const Twist&) const) &Frame::Inverse);
    frame.def_static("Identity", &Frame::Identity);
    frame.def("Integrate", &Frame::Integrate);
    frame.def(py::self * Vector());
    frame.def(py::self * Wrench());
    frame.def(py::self * Twist());
    frame.def(py::self * py::self);
    frame.def(py::self == py::self);
    frame.def(py::self != py::self);
    frame.def(py::pickle(
            [](const Frame &frm)
            { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(frm.M, frm.p);
            },
            [](py::tuple t)
            { // __setstate__
                if (t.size() != 2)
                    throw std::runtime_error("Invalid state!");

                /* Create a new C++ instance */
                Frame frm(t[0].cast<Rotation>(), t[1].cast<Vector>());
                return frm;
            }));

    m.def("Equal", (bool (*)(const Frame&, const Frame&, double eps)) &KDL::Equal,
          py::arg("a"), py::arg("b"), py::arg("eps")=epsilon);


    // --------------------
    // Global
    // --------------------
    m.def("diff", (Vector (*)(const Vector&, const Vector&, double dt)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt") = 1);
    m.def("diff", (Vector (*)(const Rotation&, const Rotation&, double dt)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt") = 1);
    m.def("diff", (Twist (*)(const Frame&, const Frame&, double dt)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt") = 1);
    m.def("diff", (Twist (*)(const Twist&, const Twist&, double dt)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt") = 1);
    m.def("diff", (Wrench (*)(const Wrench&, const Wrench&, double dt)) &KDL::diff,
          py::arg("a"), py::arg("b"), py::arg("dt") = 1);
    m.def("addDelta", (Vector (*)(const Vector&, const Vector&, double dt)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt") = 1);
    m.def("addDelta", (Rotation (*)(const Rotation&, const Vector&, double dt)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt") = 1);
    m.def("addDelta", (Frame (*)(const Frame&, const Twist&, double dt)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt") = 1);
    m.def("addDelta", (Twist (*)(const Twist&, const Twist&, double dt)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt") = 1);
    m.def("addDelta", (Wrench (*)(const Wrench&, const Wrench&, double dt)) &KDL::addDelta,
          py::arg("a"), py::arg("da"), py::arg("dt") = 1);
}
