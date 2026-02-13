#pragma once

#include <array>
#include <random>
#include <type_traits>

#include <ruckig/utils.hpp>


template<size_t DOFs, class T>
class Randomizer {
    template<class U> using Vector = ruckig::StandardVector<U, DOFs>;

    std::default_random_engine gen;
    T dist;
    std::uniform_real_distribution<double> uniform_dist;

public:
    explicit Randomizer() { }
    explicit Randomizer(T dist, int local_seed): dist(dist), uniform_dist(std::uniform_real_distribution<double>(0.0, 1.0)) {
        gen.seed(local_seed);
    }

    void fill(Vector<double>& input) {
        for (size_t dof = 0; dof < input.size(); ++dof) {
            input[dof] = dist(gen);
        }
    }

    void fill_or_zero(Vector<double>& input, double p) {
        for (size_t dof = 0; dof < input.size(); ++dof) {
            input[dof] = uniform_dist(gen) < p ? dist(gen) : 0.0;
        }
    }

    void fill(Vector<double>& input, const Vector<double>& offset) {
        for (size_t dof = 0; dof < input.size(); ++dof) {
            input[dof] = dist(gen) + std::abs(offset[dof]);
        }
    }

    void fill_min(Vector<double>& input, const Vector<double>& offset) {
        for (size_t dof = 0; dof < input.size(); ++dof) {
            input[dof] = dist(gen) - std::abs(offset[dof]);
        }
    }
};
