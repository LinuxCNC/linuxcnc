#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <deque>
#include <random>
#include "randomizer.hpp"

#include <ruckig/error.hpp>
#include <ruckig/ruckig.hpp>


using namespace ruckig;


int seed {42};
size_t number_trajectories {256 * 1024}; // Some user variable you want to be able to set
size_t position_random_1, position_random_3, random_3_high, step_through_3, random_discrete_3, random_direction_3, position_second_random_3;
size_t velocity_random_3, velocity_random_discrete_3, velocity_second_random_3;

std::normal_distribution<double> position_dist {0.0, 4.0};
std::normal_distribution<double> dynamic_dist {0.0, 0.8};
std::uniform_real_distribution<double> limit_dist {0.08, 16.0};
std::uniform_real_distribution<double> limit_dist_high {10.0, 1000000.0};
std::uniform_real_distribution<double> min_limit_dist {-16.0, -0.08};


template<class T, size_t DOFs>
class MinimalVector {
    T data[DOFs];

public:
    MinimalVector() {}
    MinimalVector(std::initializer_list<T> a) {
        std::copy_n(a.begin(), DOFs, std::begin(data));
    }

    T operator[](size_t i) const {
        return data[i];
    }

    T& operator[](size_t i) {
        return data[i];
    }

    size_t size() const {
        return DOFs;
    }

    bool operator==(const MinimalVector<T, DOFs>& rhs) const {
        for (size_t dof = 0; dof < DOFs; ++dof) {
            if (data[dof] != rhs[dof]) {
                return false;
            }
        }
        return true;
    }
};


template<class T, size_t DOFs>
class MinimalRuntimeVector {
    std::deque<T> data;

public:
    MinimalRuntimeVector() {}
    MinimalRuntimeVector(std::initializer_list<T> a) {
        data.resize(a.size());
        std::copy_n(a.begin(), a.size(), std::begin(data));
    }

    T operator[](size_t i) const {
        return data[i];
    }

    T& operator[](size_t i) {
        return data[i];
    }

    size_t size() const {
        return data.size();
    }

    void resize(size_t size) {
        data.resize(size);
    }

    bool operator==(const MinimalRuntimeVector<T, DOFs>& rhs) const {
        for (size_t dof = 0; dof < data.size(); ++dof) {
            if (data[dof] != rhs[dof]) {
                return false;
            }
        }
        return true;
    }
};


template<size_t DOFs, class OTGType>
void check_duration(OTGType& otg, InputParameter<DOFs>& input, double duration) {
    OutputParameter<DOFs> output;

    while (otg.update(input, output) == Result::Working) {
        input.current_position = output.new_position;
        input.current_velocity = output.new_velocity;
        input.current_acceleration = output.new_acceleration;
    }

    CHECK( output.trajectory.get_duration() == doctest::Approx(duration) );
}


template<size_t DOFs, class OTGType>
void check_calculation(OTGType& otg, InputParameter<DOFs>& input) {
    OutputParameter<DOFs> output;

    CAPTURE( input );

    auto result = otg.update(input, output);
    if (result == Result::ErrorTrajectoryDuration) {
        return;
    }

    CHECK( (result == Result::Working || (result == Result::Finished && output.trajectory.get_duration() < 0.005)) );
    CHECK( output.trajectory.get_duration() >= 0.0 );

    for (size_t dof = 0; dof < otg.degrees_of_freedom; ++dof) {
        CHECK_FALSE( (std::isnan(output.new_position[dof]) || std::isnan(output.new_velocity[dof]) || std::isnan(output.new_acceleration[dof])) );
    }
}


template<size_t DOFs, class OTGType>
size_t step_through_and_check_calculation(OTGType& otg, InputParameter<DOFs>& input, size_t max_number_checks) {
    OutputParameter<DOFs> output;
    OTGType otg_second {0.001};

    check_calculation<DOFs, OTGType>(otg, input);

    size_t number_checks {1};
    while (otg.update(input, output) == Result::Working) {
        input.current_position = output.new_position;
        input.current_velocity = output.new_velocity;
        input.current_acceleration = output.new_acceleration;

        // Or randomize input with small noise here?
        check_calculation<DOFs, OTGType>(otg_second, input);

        number_checks += 1;
        if (number_checks == max_number_checks) {
            break;
        }
    }

    return number_checks;
}


template<size_t DOFs, class OTGType, class OTGCompType>
void check_comparison(OTGType& otg, InputParameter<DOFs>& input, OTGCompType& otg_comparison) {
    OutputParameter<DOFs> output;

    CAPTURE( input );

    auto result = otg.update(input, output);
    if (result == Result::ErrorTrajectoryDuration) {
        return;
    }

    CHECK( (result == Result::Working || (result == Result::Finished && output.trajectory.get_duration() < 0.005)) );

    OutputParameter<DOFs> output_comparison;
    auto result_comparison = otg_comparison.update(input, output_comparison);
    CHECK( output.trajectory.get_duration() <= doctest::Approx(output_comparison.trajectory.get_duration()) );
}


template<class T>
bool array_eq(const T& first, const T& second) {
    for (size_t dof = 0; dof < first.size(); ++dof) {
        if (first[dof] != doctest::Approx(second[dof])) {
            return false;
        }
    }
    return true;
}


TEST_CASE("trajectory") {
    RuckigThrow<3> otg {0.005};
    InputParameter<3> input;
    OutputParameter<3> output;

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.0, 0.3, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    Trajectory<3> traj;
    auto result = otg.calculate(input, traj);

    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );

    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );

    SUBCASE("at-time") {
        std::array<double, 3> new_position, new_velocity, new_acceleration, new_jerk;
        output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
        CHECK( array_eq(new_position, input.current_position) );
        CHECK( array_eq(new_velocity, input.current_velocity) );
        CHECK( array_eq(new_acceleration, input.current_acceleration) );

        output.trajectory.at_time(output.trajectory.get_duration(), new_position, new_velocity, new_acceleration);
        CHECK( array_eq(new_position, input.target_position) );
        CHECK( array_eq(new_velocity, input.target_velocity) );
        CHECK( array_eq(new_acceleration, input.target_acceleration) );

        size_t new_section;
        output.trajectory.at_time(2.0, new_position, new_velocity, new_acceleration, new_jerk, new_section);
        CHECK( array_eq(new_position, {0.5, -2.6871268303, 1.0}) );
        CHECK( array_eq(new_jerk, {0.0, 0.0, -1.0}) );
        CHECK( new_section == 0 );

        output.trajectory.at_time(5.0, new_position, new_velocity, new_acceleration, new_jerk, new_section);
        CHECK( array_eq(new_jerk, {0.0, 0.0, 0.0}) );
        CHECK( new_section == 1 );
    }

    SUBCASE("at-time-single-dof") {
        RuckigThrow<1> otg {0.005};
        InputParameter<1> input;
        OutputParameter<1> output;

        input.current_position = {0.0};
        input.target_position = {1.0};
        input.max_velocity = {1.0};
        input.max_acceleration = {1.0};
        input.max_jerk = {1.0};

        Trajectory<1> traj;
        auto result = otg.calculate(input, traj);

        CHECK( result == Result::Working );
        CHECK( traj.get_duration() == doctest::Approx(3.1748) );

        std::array<double, 1> new_position{}, new_velocity{}, new_acceleration{};
        traj.at_time(0.0, new_position, new_velocity, new_acceleration);
        CHECK( array_eq(new_position, input.current_position) );

        double new_position_element, new_velocity_element, new_acceleration_element;
        traj.at_time(0.0, new_position_element, new_velocity_element, new_acceleration_element);
        CHECK( new_position_element == doctest::Approx(input.current_position[0]) );

        traj.at_time(3.1748 / 2, new_position_element, new_velocity_element, new_acceleration_element);
        CHECK( new_position_element == doctest::Approx(0.5) );
    }

    SUBCASE("independent-min-durations") {
        const auto independent_min_durations = output.trajectory.get_independent_min_durations();
        CHECK( independent_min_durations[0] == doctest::Approx(3.1748021039) );
        CHECK( independent_min_durations[1] == doctest::Approx(3.6860977315) );
        CHECK( independent_min_durations[2] == doctest::Approx(output.trajectory.get_duration()) );
    }

    SUBCASE("position-extrema") {
        const auto position_extrema = output.trajectory.get_position_extrema();
        CHECK( position_extrema[0].t_max == doctest::Approx(4.0) );
        CHECK( position_extrema[0].max == doctest::Approx(1.0) );
        CHECK( position_extrema[0].t_min == doctest::Approx(0.0) );
        CHECK( position_extrema[0].min == doctest::Approx(0.0) );

        CHECK( position_extrema[1].t_max == doctest::Approx(0.0) );
        CHECK( position_extrema[1].max == doctest::Approx(-2.0) );
        CHECK( position_extrema[1].t_min == doctest::Approx(3.2254033308) );
        CHECK( position_extrema[1].min == doctest::Approx(-3.1549193338) );

        CHECK( position_extrema[2].t_max == doctest::Approx(4.0) );
        CHECK( position_extrema[2].max == doctest::Approx(2.0) );
        CHECK( position_extrema[2].t_min == doctest::Approx(0.0) );
        CHECK( position_extrema[2].min == doctest::Approx(0.0) );
    }

    SUBCASE("first-time-at-position") {
        CHECK( output.trajectory.get_first_time_at_position(0, 0.0).value() == doctest::Approx(0.0) );
        CHECK( output.trajectory.get_first_time_at_position(0, 0.5).value() == doctest::Approx(2.0) );
        CHECK( output.trajectory.get_first_time_at_position(0, 1.0).value() == doctest::Approx(4.0) );
        CHECK( output.trajectory.get_first_time_at_position(1, -3.0).value() == doctest::Approx(2.6004877902) );
        CHECK( output.trajectory.get_first_time_at_position(1, -3.1).value() == doctest::Approx(2.8644154489) );
        CHECK( output.trajectory.get_first_time_at_position(2, 0.05).value() == doctest::Approx(0.6694329501) );

        CHECK_FALSE( output.trajectory.get_first_time_at_position(0, -1.0) );
        CHECK_FALSE( output.trajectory.get_first_time_at_position(1, -3.4) );
        CHECK_FALSE( output.trajectory.get_first_time_at_position(6, 0.0) );
    }

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {2.0, 0.3, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    CHECK_THROWS_WITH_AS( otg.update(input, output), doctest::Contains("exceeds its maximum velocity limit"), RuckigError);
    CHECK_FALSE( output.new_calculation );

    input.target_velocity = {0.2, -0.3, 0.8};
    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.new_calculation );

    input.minimum_duration = 12.0;
    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(12.0) );


    input.current_position = {1300.0, 0.0, 0.02};
    input.current_velocity = {1200.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1400.0, 0.0, 0.02};
    input.target_velocity = {0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {800.0, 1.0, 1.0};
    input.max_acceleration = {40000.0, 1.0, 1.0};
    input.max_jerk = {200000.0, 1.0, 1.0};
    input.minimum_duration = nullopt;
    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(0.167347) );

    const auto independent_min_durations = output.trajectory.get_independent_min_durations();
    CHECK( independent_min_durations[0] == doctest::Approx(output.trajectory.get_duration()) );
    CHECK( independent_min_durations[1] == doctest::Approx(0.0) );
    CHECK( independent_min_durations[2] == doctest::Approx(0.0) );
}

TEST_CASE("input-validation") {
    RuckigThrow<2> otg;
    InputParameter<2> input;

    const double nan = std::nan("");

    input.current_position = {0.0, -2.0};
    input.current_velocity = {0.0, 0.0};
    input.current_acceleration = {0.0, 0.0};
    input.target_position = {1.0, -3.0};
    input.target_velocity = {0.0, 0.3};
    input.target_acceleration = {0.0, 0.0};
    input.max_velocity = {1.0, 1.0};
    input.max_acceleration = {1.0, 1.0};
    input.max_jerk = {1.0, 1.0};

    CHECK( otg.validate_input(input) );
    CHECK( otg.validate_input<false>(input) );

    input.max_jerk = {1.0, nan};
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("maximum jerk limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input) );

    input.max_jerk = {1.0, 1.0};
    input.current_position = {1.0, nan};
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("current position"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input) );

    input.current_position = {1.0, 1.0};
    input.max_acceleration = {1.0, -1.0};
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("maximum acceleration limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input) );

    input.max_acceleration = {1.0, 1.0};
    input.max_velocity = {1.0, -1.0};
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("maximum velocity limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input) );

    input.max_velocity = {1.0, 1.0};
    input.target_velocity = {0.0, 1.3};
    CHECK( otg.validate_input(input, false, false) );
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("exceeds its maximum velocity limit"), RuckigError);
    CHECK_THROWS_WITH_AS( otg.validate_input(input, false, true), doctest::Contains("exceeds its maximum velocity limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input, false, true) );
    CHECK_FALSE( otg.validate_input<false>(input) );

    input.target_velocity = {0.0, 0.3};
    input.current_velocity = {2.0, 0.0};
    CHECK( otg.validate_input(input, false, false) );
    CHECK_THROWS_WITH_AS( otg.validate_input(input, true, false), doctest::Contains("exceeds its maximum velocity limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input, true, false) );
    CHECK_FALSE( otg.validate_input<false>(input, true, true) );
    CHECK( otg.validate_input(input) );

    input.current_velocity = {1.0, 0.0};
    input.current_acceleration = {-1.0, 0.0};
    CHECK( otg.validate_input(input) );
    CHECK( otg.validate_input(input, true, true) );

    input.current_velocity = {1.0, 0.0};
    input.current_acceleration = {1.0, 0.0};
    CHECK( otg.validate_input(input) );
    CHECK_THROWS_WITH_AS( otg.validate_input(input, true, true), doctest::Contains("will exceed its maximum velocity limit"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input, true, true) );

    input.current_velocity = {0.72, 0.0};
    input.current_acceleration = {0.72, 0.0};
    CHECK( otg.validate_input(input, true, true) );

    input.current_velocity = {0.0, 0.0};
    input.current_acceleration = {0.0, 0.0};
    input.target_velocity = {0.0, 0.72};
    input.target_acceleration = {0.0, 0.72};
    CHECK( otg.validate_input(input) );
    CHECK( otg.validate_input<false>(input) );

    input.target_velocity = {0.0, 1.0};
    input.target_acceleration = {0.0, 1.0};
    CHECK( otg.validate_input(input) );

    input.target_velocity = {0.0, 1.0};
    input.target_acceleration = {0.0, -0.0001};
    CHECK_THROWS_WITH_AS( otg.validate_input(input), doctest::Contains("will exceed its maximum velocity"), RuckigError);
    CHECK_FALSE( otg.validate_input<false>(input) );
}

TEST_CASE("enabled") {
    RuckigThrow<3> otg {0.005};
    InputParameter<3> input;
    OutputParameter<3> output;

    input.enabled = {true, false, false};
    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.1, 0.0};
    input.current_acceleration = {0.0, 0.0, -0.2};
    input.target_position = {1.0, -3.0, 2.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    Result result = otg.update(input, output);
    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(3.1748021039) );

    std::array<double, 3> new_position, new_velocity, new_acceleration;
    output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
    array_eq(new_position, input.current_position);
    array_eq(new_velocity, input.current_velocity);
    array_eq(new_acceleration, input.current_acceleration);

    output.trajectory.at_time(output.trajectory.get_duration(), new_position, new_velocity, new_acceleration);
    array_eq(new_position, {input.target_position[0], -1.6825197896, -1.0079368399});

    // Make sure that disabled DoFs overwrite prior blocks
    input.enabled = {true, true, true};
    input.current_position = {0.0, 0.0, 0.0};
    input.target_position = {100.0, -3000.0, 2000.0};
    input.target_velocity = {1.0, 1.0, 1.0};
    result = otg.update(input, output);

    input.enabled = {false, false, true};
    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.2, 0.0};
    input.current_acceleration = {0.0, 0.2, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.0, 0.0, 0.2};
    input.target_acceleration = {0.0, 0.0, -0.1};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    result = otg.update(input, output);
    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(3.6578610221) );
}

TEST_CASE("phase-synchronization") {
    RuckigThrow<3> otg {0.005};
    InputParameter<3> input;
    OutputParameter<3> output;
    Trajectory<3> traj;
    std::array<double, 3> new_position, new_velocity, new_acceleration;

    input.current_position = {0.0, -2.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};
    input.synchronization = Synchronization::Phase;
    auto result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    result = otg.update(input, output);
    output.trajectory.at_time(1.0, new_position, new_velocity, new_acceleration);
    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );
    CHECK( array_eq(new_position, {0.0833333333, -2.0833333333, 0.1666666667}) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][1].t) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][2].t) );

    input.current_position = {0.0, -2.0, 0.0};
    input.target_position = {10.0, -3.0, 2.0};
    input.max_velocity = {10.0, 2.0, 1.0};
    input.max_acceleration = {10.0, 2.0, 1.0};
    input.max_jerk = {10.0, 2.0, 1.0};
    result = otg.update(input, output);
    output.trajectory.at_time(1.0, new_position, new_velocity, new_acceleration);
    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );
    CHECK( array_eq(new_position, {0.8333333333, -2.0833333333, 0.1666666667}) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][1].t) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][2].t) );

    // Test equal start and target state
    input.current_position = {1.0, -2.0, 3.0};
    input.target_position = {1.0, -2.0, 3.0};
    result = otg.update(input, output);
    output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
    CHECK( result == Result::Finished );
    CHECK( output.trajectory.get_duration() == doctest::Approx(0.0) );
    CHECK( array_eq(new_position, {1.0, -2.0, 3.0}) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][1].t) );
    CHECK( array_eq(output.trajectory.get_profiles()[0][0].t, output.trajectory.get_profiles()[0][2].t) );

    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {0.0, 0.0, 0.0};
    input.target_velocity = {0.2, 0.3, 0.4};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {0.0, 0.0, 0.01};
    input.target_velocity = {0.2, 0.3, 0.4};
    input.target_acceleration = {0.0, 0.0, 0.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.4, 0.15, 0.2};
    input.current_acceleration = {0.8, 0.3, 0.4};
    input.target_position = {0.0, 0.0, 0.0};
    input.target_velocity = {0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.max_velocity = {1.0, 0.2, 1.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.current_position = {0.0, 0.02, 1.0};
    input.current_velocity = {-0.2, 0.15, 0.2};
    input.current_acceleration = {-0.4, 0.3, 0.4};
    input.target_position = {0.03, 0.0, 0.0};
    input.target_velocity = {-0.02, 0.015, 0.02};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};
    input.control_interface = ControlInterface::Velocity;
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.max_jerk = {1.0, 0.1, 1.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.target_acceleration = {0.01, 0.0, 0.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );

    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {0.0, 0.0, 0.0};
    input.target_velocity = {0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
}

TEST_CASE("discretization") {
    RuckigThrow<3> otg {0.01};
    InputParameter<3> input;
    OutputParameter<3> output;

    input.current_position = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.2, 0.2, 0.2};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {2.0, 2.0, 2.0};
    input.max_jerk = {1.8, 2.4, 2.0};
    input.duration_discretization = DurationDiscretization::Discrete;

    Trajectory<3> traj;
    std::array<double, 3> new_position, new_velocity, new_acceleration;
    auto result = otg.calculate(input, traj);

    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.5) );

    result = otg.update(input, output);
    output.trajectory.at_time(4.5, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {1.0, -3.0, 2.0}) );
}

TEST_CASE("per-dof-setting") {
    RuckigThrow<3> otg {0.005};
    InputParameter<3> input;
    Trajectory<3> traj;

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.0, 0.3, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    auto result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );

    std::array<double, 3> new_position, new_velocity, new_acceleration;
    traj.at_time(2.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {0.5, -2.6871268303, 1.0}) );


    input.control_interface = ControlInterface::Velocity;

    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(1.095445115) );

    traj.at_time(1.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {0.0, -1.8641718534, 0.0}) );


    input.per_dof_control_interface = StandardVector<ControlInterface, 3> {ControlInterface::Position, ControlInterface::Velocity, ControlInterface::Position};

    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );

    traj.at_time(2.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {0.5, -1.8528486838, 1.0}) );


    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::Time, Synchronization::None, Synchronization::Time};

    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );

    traj.at_time(2.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {0.5, -1.5643167673, 1.0}) );


    input.control_interface = ControlInterface::Position;
    input.per_dof_control_interface = nullopt;
    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::None, Synchronization::Time, Synchronization::Time};


    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(4.0) );

    traj.at_time(2.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, {0.7482143874, -2.6871268303, 1.0}) );


    auto independent_min_durations = traj.get_independent_min_durations();
    traj.at_time(independent_min_durations[0], new_position, new_velocity, new_acceleration);
    CHECK( new_position[0] == doctest::Approx(input.target_position[0]) );
    traj.at_time(independent_min_durations[1], new_position, new_velocity, new_acceleration);
    CHECK( new_position[1] == doctest::Approx(-3.0890156397) );
    traj.at_time(independent_min_durations[2], new_position, new_velocity, new_acceleration);
    CHECK( new_position[2] == doctest::Approx(input.target_position[2]) );


    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {35, 35, 35};
    input.target_velocity = {125, 125, 100};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {125, 125, 100};
    input.max_acceleration = {2000, 2000, 2000};
    input.max_jerk = {20000, 20000, 20000};
    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::Time, Synchronization::Time, Synchronization::None};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(0.4207106781) );

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.2, 0.0};
    input.current_acceleration = {0.0, 0.2, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.0, 0.0, 0.2};
    input.target_acceleration = {0.0, 0.0, -0.1};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};
    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::None, Synchronization::None, Synchronization::Time};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(3.7885667284) );

    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::None, Synchronization::Time, Synchronization::None};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(3.7885667284) );

    input.enabled = {true, false, true};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(3.6578610221) );

    input.current_position = {0.0, 0.0, 0.0};
    input.current_velocity = {0.2, 0.0, -0.1};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -0.2, -0.5};
    input.target_velocity = {0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};
    input.per_dof_synchronization = StandardVector<Synchronization, 3> {Synchronization::Phase, Synchronization::None, Synchronization::Phase};
    input.enabled = {true, true, true};
    result = otg.calculate(input, traj);
    CHECK( result == Result::Working );
    CHECK( traj.get_duration() == doctest::Approx(2.848387279) );
    CHECK_FALSE( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][1].t) );
    CHECK( array_eq(traj.get_profiles()[0][0].t, traj.get_profiles()[0][2].t) );
}

TEST_CASE("dynamic-dofs") {
    RuckigThrow<DynamicDOFs> otg {3, 0.005};
    InputParameter<DynamicDOFs> input {3};
    OutputParameter<DynamicDOFs> output {3};

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 2.0};
    input.target_velocity = {0.0, 0.3, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {1.0, 1.0, 1.0};
    input.max_jerk = {1.0, 1.0, 1.0};

    auto result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );

    std::vector<double> new_position(3), new_velocity(3), new_acceleration(3);
    output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
    CHECK( array_eq(new_position, input.current_position) );
    CHECK( array_eq(new_velocity, input.current_velocity) );
    CHECK( array_eq(new_acceleration, input.current_acceleration) );
}

TEST_CASE("zero-limits") {
    RuckigThrow<3> otg {0.005};
    InputParameter<3> input;
    OutputParameter<3> output;

    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {0.2, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0};
    input.target_position = {1.0, -3.0, 0.0};
    input.target_velocity = {0.2, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0};
    input.max_velocity = {1.0, 1.0, 1.0};
    input.max_acceleration = {0.0, 1.0, 0.0};
    input.max_jerk = {0.0, 1.0, 0.0};

    auto result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(5.0) );


    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {-0.2, 0.0, 0.0};
    input.current_acceleration = {1.0, 0.0, 0.0};
    input.target_position = {0.4, -3.0, 0.0};
    input.target_velocity = {0.8, 0.0, 0.0};
    input.target_acceleration = {1.0, 0.0, 0.0};
    input.max_velocity = {1.0, 200.0, 1.0};
    input.max_acceleration = {1.0, 200.0, 0.0};
    input.max_jerk = {0.0, 200.0, 0.0};

    CHECK_THROWS_WITH_AS( otg.update(input, output), doctest::Contains("zero limits conflict in step 1"), RuckigError);


    input.target_position = {0.3, -3.0, 0.0};
    input.max_velocity = {1.0, 2.0, 1.0};
    input.max_acceleration = {1.0, 2.0, 0.0};
    input.max_jerk = {0.0, 2.0, 0.0};

    CHECK_THROWS_WITH_AS( otg.update(input, output), doctest::Contains("zero limits conflict with other"), RuckigError);


    input.control_interface = ControlInterface::Velocity;
    input.current_position = {0.0, -2.0, 0.0};
    input.current_velocity = {-0.2, 0.0, 0.0};
    input.current_acceleration = {1.0, 0.0, 0.2};
    input.target_position = {0.4, -3.0, 0.0};
    input.target_velocity = {0.9, 0.5, 0.4};
    input.target_acceleration = {1.0, 0.0, 0.2};
    input.max_velocity = {1.0, 2.0, 1.0};
    input.max_acceleration = {1.0, 2.0, 6.0};
    input.max_jerk = {0.0, 2.0, 0.0};

    CHECK_THROWS_WITH_AS( otg.update(input, output), doctest::Contains("zero limits conflict with other"), RuckigError);

    input.max_jerk = {1.0, 2.0, 0.0};

    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(2.0) );

    input.max_jerk = {0.0, 2.0, 20.0};

    result = otg.update(input, output);

    CHECK( result == Result::Working );
    CHECK( output.trajectory.get_duration() == doctest::Approx(1.1) );
}

TEST_CASE("custom-vector-type") {
    SUBCASE("DOFs compile-time") {
        RuckigThrow<3, MinimalVector> otg {0.005};
        InputParameter<3, MinimalVector> input;
        OutputParameter<3, MinimalVector> output;

        input.current_position = {0.0, -2.0, 0.0};
        input.current_velocity = {0.0, 0.0, 0.0};
        input.current_acceleration = {0.0, 0.0, 0.0};
        input.target_position = {1.0, -3.0, 2.0};
        input.target_velocity = {0.0, 0.3, 0.0};
        input.target_acceleration = {0.0, 0.0, 0.0};
        input.max_velocity = {1.0, 1.0, 1.0};
        input.max_acceleration = {1.0, 1.0, 1.0};
        input.max_jerk = {1.0, 1.0, 1.0};

        auto result = otg.update(input, output);

        CHECK( result == Result::Working );
        CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );

        MinimalVector<double, 3> new_position, new_velocity, new_acceleration;
        output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
        CHECK( new_position[0] == doctest::Approx(input.current_position[0]) );
        CHECK( new_position[1] == doctest::Approx(input.current_position[1]) );
        CHECK( new_position[2] == doctest::Approx(input.current_position[2]) );
    }

    SUBCASE("DOFs run-time") {
        RuckigThrow<DynamicDOFs, MinimalRuntimeVector> otg {3, 0.005};
        InputParameter<DynamicDOFs, MinimalRuntimeVector> input {3};
        OutputParameter<DynamicDOFs, MinimalRuntimeVector> output {3};

        input.current_position = {0.0, -2.0, 0.0};
        input.current_velocity = {0.0, 0.0, 0.0};
        input.current_acceleration = {0.0, 0.0, 0.0};
        input.target_position = {1.0, -3.0, 2.0};
        input.target_velocity = {0.0, 0.3, 0.0};
        input.target_acceleration = {0.0, 0.0, 0.0};
        input.max_velocity = {1.0, 1.0, 1.0};
        input.max_acceleration = {1.0, 1.0, 1.0};
        input.max_jerk = {1.0, 1.0, 1.0};

        auto result = otg.update(input, output);

        CHECK( result == Result::Working );
        CHECK( output.trajectory.get_duration() == doctest::Approx(4.0) );

        MinimalRuntimeVector<double, DynamicDOFs> new_position, new_velocity, new_acceleration;
        new_position.resize(3);
        new_velocity.resize(3);
        new_acceleration.resize(3);
        output.trajectory.at_time(0.0, new_position, new_velocity, new_acceleration);
        CHECK( new_position[0] == doctest::Approx(input.current_position[0]) );
        CHECK( new_position[1] == doctest::Approx(input.current_position[1]) );
        CHECK( new_position[2] == doctest::Approx(input.current_position[2]) );
    }
}

TEST_CASE("random-discrete-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    std::uniform_int_distribution<int> position_discrete_dist(-1, 1);
    std::uniform_int_distribution<int> dynamic_discrete_dist(-1, 1);
    std::uniform_int_distribution<int> limit_discrete_dist(1, 2);

    Randomizer<DOFs, decltype(position_discrete_dist)> p { position_discrete_dist, seed };
    Randomizer<DOFs, decltype(dynamic_discrete_dist)> d { dynamic_discrete_dist, seed + 1 };
    Randomizer<DOFs, decltype(limit_discrete_dist)> l { limit_discrete_dist, seed + 2 };

    for (size_t i = 0; i < random_discrete_3; ++i) {
        if (i < random_discrete_3 / 2) {
            input.synchronization = Synchronization::Phase;
        } else {
            input.synchronization = Synchronization::Time;
        }

        p.fill(input.current_position);
        d.fill(input.current_velocity);
        d.fill(input.current_acceleration);
        p.fill(input.target_position);
        d.fill(input.target_velocity);
        d.fill(input.target_acceleration);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("position-random-1") {
    const size_t DOFs = 1;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 1 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 2 };

    for (size_t i = 0; i < position_random_1; ++i) {
        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("velocity-random-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;
    input.control_interface = ControlInterface::Velocity;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };

    input.current_position = {0.0, 0.0, 0.0};

    for (size_t i = 0; i < velocity_random_3; ++i) {
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        check_calculation(otg, input);
    }
}

TEST_CASE("velocity-random-discrete-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;
    input.control_interface = ControlInterface::Velocity;

    std::uniform_int_distribution<int> position_discrete_dist(-1, 1);
    std::uniform_int_distribution<int> dynamic_discrete_dist(-1, 1);
    std::uniform_int_distribution<int> limit_discrete_dist(1, 2);

    Randomizer<DOFs, decltype(position_discrete_dist)> p { position_discrete_dist, seed };
    Randomizer<DOFs, decltype(dynamic_discrete_dist)> d { dynamic_discrete_dist, seed + 1 };
    Randomizer<DOFs, decltype(limit_discrete_dist)> l { limit_discrete_dist, seed + 2 };

    for (size_t i = 0; i < velocity_random_discrete_3; ++i) {
        if (i < velocity_random_discrete_3 / 2) {
            input.synchronization = Synchronization::Phase;
        } else {
            input.synchronization = Synchronization::Time;
        }

        p.fill(input.current_position);
        d.fill(input.current_velocity);
        d.fill(input.current_acceleration);
        p.fill(input.target_position);
        d.fill(input.target_velocity);
        d.fill(input.target_acceleration);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("velocity-second-random-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;
    input.control_interface = ControlInterface::Velocity;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };
    Randomizer<DOFs, decltype(min_limit_dist)> min_l { min_limit_dist, seed + 6 };

    input.current_position = {0.0, 0.0, 0.0};
    // input.min_acceleration = {0.0, 0.0, 0.0}; // To "activate" std::optional

    for (size_t i = 0; i < velocity_second_random_3; ++i) {
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_acceleration, input.target_acceleration);
        // min_l.fill_min(*input.min_acceleration, input.target_acceleration);

        check_calculation(otg, input);
    }
}

TEST_CASE("random-3-high") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist_high)> l { limit_dist_high, seed + 5 };

    for (size_t i = 0; i < random_3_high; ++i) {
        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.1);
        d.fill_or_zero(input.current_acceleration, 0.1);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.1);
        d.fill_or_zero(input.target_acceleration, 0.1);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("step-through-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.01};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };

    for (size_t i = 0; i < step_through_3; ++i) {
        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        i += step_through_and_check_calculation(otg, input, 1000);
    }
}

TEST_CASE("random-direction-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };
    Randomizer<DOFs, decltype(min_limit_dist)> min_l { min_limit_dist, seed + 5 };

    // To "activate" std::optional
    input.min_velocity = StandardVector<double, DOFs> {0.0, 0.0, 0.0};
    input.min_acceleration = StandardVector<double, DOFs> {0.0, 0.0, 0.0};

    for (size_t i = 0; i < random_direction_3; ++i) {
        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);
        min_l.fill_min(*input.min_velocity, input.target_velocity);
        min_l.fill_min(*input.min_acceleration, input.target_acceleration);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("position-random-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };

    for (size_t i = 0; i < position_random_3; ++i) {
        if (i < position_random_3 / 2) {
            input.synchronization = Synchronization::Phase;
        } else {
            input.synchronization = Synchronization::Time;
        }

        if (i < position_random_3 / 20) {
            input.duration_discretization = DurationDiscretization::Discrete;
        } else {
            input.duration_discretization = DurationDiscretization::Continuous;
        }

        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);
        l.fill(input.max_jerk);

        // if (i % 1000 == 0) {
        //     const double factor = 1e3;
        //     for (size_t d = 0; d < DOFs; ++d) {
        //         input.current_position[d] *= factor;
        //         input.current_velocity[d] *= factor;
        //         input.current_acceleration[d] *= factor;
        //         input.target_position[d] *= factor;
        //         input.target_velocity[d] *= factor;
        //         input.target_acceleration[d] *= factor;
        //         input.max_velocity[d] *= factor;
        //         input.max_acceleration[d] *= factor;
        //         input.max_jerk[d] *= factor;
        //     }
        // }

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}

TEST_CASE("position-second-random-3") {
    const size_t DOFs = 3;
    RuckigThrow<DOFs> otg {0.005};
    InputParameter<DOFs> input;

    Randomizer<DOFs, decltype(position_dist)> p { position_dist, seed + 3 };
    Randomizer<DOFs, decltype(dynamic_dist)> d { dynamic_dist, seed + 4 };
    Randomizer<DOFs, decltype(limit_dist)> l { limit_dist, seed + 5 };

    for (size_t i = 0; i < position_second_random_3; ++i) {
        if (i < position_second_random_3 / 2) {
            input.synchronization = Synchronization::Phase;
        } else {
            input.synchronization = Synchronization::Time;
        }

        if (i < position_second_random_3 / 20) {
            input.duration_discretization = DurationDiscretization::Discrete;
        } else {
            input.duration_discretization = DurationDiscretization::Continuous;
        }

        p.fill(input.current_position);
        d.fill_or_zero(input.current_velocity, 0.9);
        d.fill_or_zero(input.current_acceleration, 0.8);
        p.fill(input.target_position);
        d.fill_or_zero(input.target_velocity, 0.7);
        d.fill_or_zero(input.target_acceleration, 0.6);
        l.fill(input.max_velocity, input.target_velocity);
        l.fill(input.max_acceleration, input.target_acceleration);

        if (!otg.validate_input<false>(input)) {
            --i;
            continue;
        }

        check_calculation(otg, input);
    }
}


int main(int argc, char** argv) {
    doctest::Context context;

    if (argc > 1 && std::isdigit(argv[1][0])) {
        number_trajectories = std::stol(argv[1]);
    }
    if (argc > 2 && std::isdigit(argv[2][0])) {
        seed = std::stoi(argv[2]);
    }

    context.applyCommandLine(argc, argv);

    position_random_1 = number_trajectories / 10;
    position_second_random_3 = std::min<size_t>(250000, number_trajectories / 25);
    step_through_3 = 0; // number_trajectories / 20;
    random_direction_3 = std::min<size_t>(250000, number_trajectories / 50);
    random_discrete_3 = std::min<size_t>(250000, number_trajectories / 10);
    velocity_random_3 = number_trajectories / 10;
    velocity_random_discrete_3 = std::min<size_t>(250000, number_trajectories / 10);
    velocity_second_random_3 = std::min<size_t>(250000, number_trajectories / 25);

    const size_t remainder = number_trajectories - (position_random_1 + step_through_3 + random_direction_3 + velocity_random_3 + random_discrete_3 + position_second_random_3 + velocity_second_random_3); // 1. Normal, 2. High
    position_random_3 = (size_t)(remainder * 95/100);
    random_3_high = (size_t)(remainder * 5/100);

    std::cout << "<number_trajectories>" << std::endl;
    std::cout << "\tPosition (1 DoF): " << position_random_1 << std::endl;
    std::cout << "\tPosition (3 DoF): " << position_random_3 << std::endl;
    std::cout << "\tPosition Discrete (3 DoF): " << random_discrete_3 << std::endl;
    std::cout << "\tPosition High Limits (3 DoF): " << random_3_high << std::endl;
    std::cout << "\tPosition Second Order (3 DoF): " << position_second_random_3 << std::endl;
    std::cout << "\tPosition Step Through (3 DoF): " << step_through_3 << std::endl;
    std::cout << "\tVelocity (3 DoF): " << velocity_random_3 << std::endl;
    std::cout << "\tVelocity Discrete (3 DoF): " << velocity_random_discrete_3 << std::endl;
    std::cout << "\tVelocity Second Order (3 DoF): " << velocity_second_random_3 << std::endl;
    std::cout << "Total: " << number_trajectories << std::endl;

    return context.run();
}
