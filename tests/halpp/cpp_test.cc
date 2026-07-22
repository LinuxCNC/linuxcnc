// C++ smoke test for hal.hh: native (non-Python) consumer of the C++ API.
// Compile against the RIP tree, run under a live HAL session.
#include <cstdio>
#include <cmath>
#include "hal.hh"

namespace hal = linuxcnc::hal;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if(cond) printf("ok - %s\n", msg); \
    else { printf("FAIL - %s\n", msg); failures++; } \
} while(0)

int main()
{
    try {
        hal::component c("halcpp-test");

        // Typed pins via compile-time API
        auto out  = c.newpin<rtapi_real>("out", hal::dir::OUT);
        auto in   = c.newpin<rtapi_s32>("in", hal::dir::IN);
        auto cnt  = c.newpin<rtapi_uint>("count", hal::dir::IO);
        auto flag = c.newpin<rtapi_bool>("flag", hal::dir::OUT);

        // Typed params
        auto gain = c.newparam<rtapi_real>("gain", hal::dir::RW, 1.5);
        auto mode = c.newparam<rtapi_s32>("mode", hal::dir::RO, 3);
        auto limit = c.newparam<rtapi_s32>("limit", hal::dir::RW, 0);

        // Handle-based set/get (inline accessor expansion)
        out = 42.5;
        CHECK(fabs(out.get() - 42.5) < 1e-9, "typed pin<double> set/get");
        flag = true;
        CHECK(flag.get(), "typed pin<bool> set/get");
        cnt = (rtapi_uint)1 << 60;
        CHECK(cnt.get() == ((rtapi_uint)1 << 60), "typed pin<uint64_t> 64-bit value");
        CHECK(mode.get() == 3, "param default value");

        // Component item access (runtime typed)
        c.setitem("in", -777);
        CHECK(std::get<rtapi_s32>(c.getitem("in")) == -777, "setitem/getitem int32");
        CHECK(std::get<rtapi_real>(c.getitem("gain")) == 1.5, "getitem param double");
        CHECK(c.contains("flag"), "contains()");

        c.ready();

        // Signals
        CHECK(hal::signal_new("halcpp-sig", HAL_S32) == 0, "signal_new");
        CHECK(hal::link("halcpp-test.in", "halcpp-sig") == 0, "link");

#ifdef HALXX_WITH_QUERY_API
        CHECK(hal::component_exists("halcpp-test"), "component_exists");
        CHECK(hal::component_is_ready("halcpp-test"), "component_is_ready");
        hal::set_signal("halcpp-sig", 42);
        CHECK(std::get<rtapi_s32>(hal::get_value("halcpp-sig")) == 42, "set_signal/get_value");
        CHECK(in.get() == 42, "handle reads linked signal value");
        CHECK(hal::pin_has_writer("halcpp-test.in") == false, "pin_has_writer false");

        hal::set_value("halcpp-test.gain", 3.0);
        CHECK(std::get<rtapi_real>(hal::get_value("halcpp-test.gain")) == 3.0, "set_value/get_value param");

        // Error paths
        bool threw = false;
        try { hal::get_value("no-such-thing"); } catch(const std::invalid_argument &) { threw = true; }
        CHECK(threw, "get_value missing name throws");
        threw = false;
        try { hal::set_value("halcpp-test.mode", 5); } catch(const std::invalid_argument &) { threw = true; }
        CHECK(threw, "set_value on RO param throws");
        threw = false;
        try { hal::set_value("halcpp-test.limit", 1e300); } catch(const std::out_of_range &) { threw = true; }
        CHECK(threw, "set_value range check throws (no mutex wedge)");
        // Session must still be alive after the throw
        CHECK(hal::component_exists("halcpp-test"), "HAL session alive after exception");
#else
        printf("note: query API not present, skipping by-name checks\n");
#endif

        c.exit();
#ifdef HALXX_WITH_QUERY_API
        CHECK(!hal::component_exists("halcpp-test"), "exit removes component");
#endif
    } catch(const std::exception &e) {
        printf("FAIL - unexpected exception: %s\n", e.what());
        failures++;
    }

    printf(failures ? "%d FAILURES\n" : "ALL C++ TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
