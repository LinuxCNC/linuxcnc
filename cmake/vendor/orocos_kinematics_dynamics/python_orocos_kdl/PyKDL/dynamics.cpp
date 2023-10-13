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


#include <iostream>
#include <iomanip>
#include <kdl/chaindynparam.hpp>
#include <kdl/jntspaceinertiamatrix.hpp>
#include <kdl/kinfam_io.hpp>
#include "PyKDL.h"

namespace py = pybind11;
using namespace KDL;


void init_dynamics(pybind11::module &m)
{
    // --------------------
    // JntSpaceInertiaMatrix
    // --------------------
    py::class_<JntSpaceInertiaMatrix> jnt_space_inertia_matrix(m, "JntSpaceInertiaMatrix");
    jnt_space_inertia_matrix.def(py::init<>());
    jnt_space_inertia_matrix.def(py::init<int>());
    jnt_space_inertia_matrix.def(py::init<const JntSpaceInertiaMatrix&>());
    jnt_space_inertia_matrix.def("resize", &JntSpaceInertiaMatrix::resize);
    jnt_space_inertia_matrix.def("rows", &JntSpaceInertiaMatrix::rows);
    jnt_space_inertia_matrix.def("columns", &JntSpaceInertiaMatrix::columns);
    jnt_space_inertia_matrix.def("__getitem__", [](const JntSpaceInertiaMatrix &jm, std::tuple<int, int> idx)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || (unsigned int)i >= jm.rows() || j < 0 || (unsigned int)j >= jm.columns())
            throw py::index_error("Inertia index out of range");

        return jm((unsigned int)i, (unsigned int)j);
    });
    jnt_space_inertia_matrix.def("__setitem__", [](JntSpaceInertiaMatrix &jm, std::tuple<int, int> idx, double value)
    {
        int i = std::get<0>(idx);
        int j = std::get<1>(idx);
        if (i < 0 || (unsigned int)i >= jm.rows() || j < 0 || (unsigned int)j >= jm.columns())
            throw py::index_error("Inertia index out of range");

        jm((unsigned int)i, (unsigned int)j) = value;
    });
    jnt_space_inertia_matrix.def("__repr__", [](const JntSpaceInertiaMatrix &jm)
    {
        std::ostringstream oss;
        oss << jm;
        return oss.str();
    });
    jnt_space_inertia_matrix.def(py::self == py::self);

    m.def("Add", (void (*)(const JntSpaceInertiaMatrix&, const JntSpaceInertiaMatrix&, JntSpaceInertiaMatrix&)) &KDL::Add);
    m.def("Subtract", (void (*)(const JntSpaceInertiaMatrix&, const JntSpaceInertiaMatrix&,JntSpaceInertiaMatrix&)) &KDL::Subtract);
    m.def("Multiply", (void (*)(const JntSpaceInertiaMatrix&, const double&, JntSpaceInertiaMatrix&)) &KDL::Multiply);
    m.def("Divide", (void (*)(const JntSpaceInertiaMatrix&, const double&, JntSpaceInertiaMatrix&)) &KDL::Divide);
    m.def("Multiply", (void (*)(const JntSpaceInertiaMatrix&, const JntArray&, JntArray&)) &KDL::Multiply);
    m.def("SetToZero", (void (*)(JntSpaceInertiaMatrix&)) &KDL::SetToZero);
    m.def("Equal", (bool (*)(const JntSpaceInertiaMatrix&, const JntSpaceInertiaMatrix&, double)) &KDL::Equal,
          py::arg("src1"), py::arg("src2"), py::arg("eps")=epsilon);


    // --------------------
    // ChainDynParam
    // --------------------
    py::class_<ChainDynParam> chain_dyn_param(m, "ChainDynParam");
    chain_dyn_param.def(py::init<const Chain&, Vector>());
    chain_dyn_param.def("JntToCoriolis", &ChainDynParam::JntToCoriolis);
    chain_dyn_param.def("JntToMass", &ChainDynParam::JntToMass);
    chain_dyn_param.def("JntToGravity", &ChainDynParam::JntToGravity);
}
