#pragma once

#include <exception>
#include <stdexcept>
#include <string>


namespace ruckig {

//! The base class for all exceptions
struct RuckigError: public std::runtime_error {
    explicit RuckigError(const std::string& message): std::runtime_error("\n[ruckig] " + message) { }
};

}
