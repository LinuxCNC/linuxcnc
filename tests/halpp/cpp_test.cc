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
        auto in   = c.newpin<rtapi_sint>("in", hal::dir::IN);
        auto cnt  = c.newpin<rtapi_uint>("count", hal::dir::IO);
        auto flag = c.newpin<rtapi_bool>("flag", hal::dir::OUT);

        // Port pins: separate handle type, byte-stream access
        auto pout = c.newport("pout", hal::dir::OUT);
        hal::port pin_in;
        c.add_port("pin", hal::dir::IN, pin_in);

        // Typed params
        auto gain = c.newparam<rtapi_real>("gain", hal::dir::RW, 1.5);
        auto mode = c.newparam<rtapi_sint>("mode", hal::dir::RO, 3);
        auto limit = c.newparam<rtapi_uint>("limit", hal::dir::RW, 0);

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
        CHECK(std::get<rtapi_sint>(c.getitem("in")) == -777, "setitem/getitem negative sint");
        c.setitem("in", -((rtapi_sint)1 << 40));
        CHECK(std::get<rtapi_sint>(c.getitem("in")) == -((rtapi_sint)1 << 40), "setitem/getitem 64-bit sint");
        CHECK(std::get<rtapi_real>(c.getitem("gain")) == 1.5, "getitem param double");
        {
            bool threw = false;
            try { c.setitem("count", -1); } catch(const std::out_of_range &) { threw = true; }
            CHECK(threw && cnt.get() == ((rtapi_uint)1 << 60), "setitem range check throws, value kept");
        }
        CHECK(c.contains("flag"), "contains()");

        c.ready();

        // Signals
        CHECK(hal::signal_new("halcpp-sig", HAL_SINT) == 0, "signal_new");
        CHECK(hal::link("halcpp-test.in", "halcpp-sig") == 0, "link");

        CHECK(hal::component_exists("halcpp-test"), "component_exists");
        CHECK(hal::component_is_ready("halcpp-test"), "component_is_ready");
        hal::set_signal("halcpp-sig", 42);
        CHECK(std::get<rtapi_sint>(hal::get_value("halcpp-sig")) == 42, "set_signal/get_value");
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
        try { hal::set_value("halcpp-test.limit", -1); } catch(const std::out_of_range &) { threw = true; }
        CHECK(threw, "set_value range check throws (no mutex wedge)");
        // Session must still be alive after the throw
        CHECK(hal::component_exists("halcpp-test"), "HAL session alive after exception");

        // Ports: the linking signal owns the buffer, "sets" sizes it
        CHECK(!pout.has_buffer() && !pout.write("x", 1), "unlinked port has no buffer");
        CHECK(hal::signal_new("halcpp-port", HAL_PORT) == 0, "port signal_new");
        CHECK(hal::link("halcpp-test.pout", "halcpp-port") == 0, "link port writer");
        CHECK(hal::link("halcpp-test.pin", "halcpp-port") == 0, "link port reader");
        hal::set_signal("halcpp-port", 8);
        CHECK(pout.size() == 8 && pin_in.size() == 8, "port buffer sized by set_signal");
        CHECK(std::get<rtapi_uint>(hal::get_value("halcpp-port")) == 8, "port signal reads as its buffer size");
        threw = false;
        try { hal::get_value("halcpp-test.pin"); } catch(const std::invalid_argument &) { threw = true; }
        CHECK(threw, "port pin by name has no value");
        threw = false;
        try { c.getitem("pin"); } catch(const std::invalid_argument &) { threw = true; }
        CHECK(threw, "getitem on a port pin throws");
        CHECK(pout.write(std::vector<char>{'a', 'b', 'c'}), "port write");
        CHECK(pin_in.readable() == 3, "port readable");
        auto got = pin_in.read(3);
        CHECK(got && std::string(got->begin(), got->end()) == "abc", "port read");
        CHECK(!pin_in.read(1).has_value(), "read of an empty port is empty");
        threw = false;
        try { c.setitem("pin", 1); } catch(const std::invalid_argument &) { threw = true; }
        CHECK(threw, "setitem on a port pin throws");

        // Streams. Reading from the creating component's own handle is
        // enough here; the create/attach pair needs two processes and
        // lives in stream_writer.py / stream_reader.py.
        {
            hal::stream s(c, 0x48535431, 4, "bfsu");
            CHECK(s.element_count() == 4, "stream element_count");
            CHECK(s.typestring() == "bfsu", "stream typestring");
            CHECK(s.element_type(2) == HAL_SINT, "stream element_type");
            CHECK(s.maxdepth() == 4, "stream maxdepth");

            bool ok = true;
            for(int i = 0; i < 3; i++) {
                ok = ok && s.writable();
                s.write({(rtapi_bool)(i % 2), (rtapi_real)i, (rtapi_sint)i, (rtapi_uint)i});
            }
            CHECK(ok, "3 samples written");
            CHECK(!s.writable(), "stream full");

            bool threw = false;
            try { s.write({(rtapi_bool)1}); } catch(const std::invalid_argument &) { threw = true; }
            CHECK(threw, "wrong element count throws");
            threw = false;
            try { s.write({(rtapi_bool)0, (rtapi_real)0, (rtapi_sint)0, (rtapi_sint)-1}); }
            catch(const std::out_of_range &) { threw = true; }
            CHECK(threw, "out-of-range element throws");

            ok = true;
            for(int i = 0; i < 3; i++) {
                auto sample = s.read();
                ok = ok && sample && sample->size() == 4;
                ok = ok && std::get<rtapi_sint>((*sample)[2]) == i;
                ok = ok && s.sampleno() == (unsigned)(i + 1);
            }
            CHECK(ok, "3 samples read back");
            CHECK(!s.read().has_value(), "read of an empty stream is empty");
            CHECK(s.num_underruns() == 1, "underrun counted");
        }

        c.exit();
        CHECK(!hal::component_exists("halcpp-test"), "exit removes component");
    } catch(const std::exception &e) {
        printf("FAIL - unexpected exception: %s\n", e.what());
        failures++;
    }

    printf(failures ? "%d FAILURES\n" : "ALL C++ TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
