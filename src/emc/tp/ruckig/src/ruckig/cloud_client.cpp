#include <string>

#include <httplib/httplib.h>

#include <ruckig/calculator_cloud.hpp>


namespace ruckig {

CloudClient::CloudClient() {
    cli = std::make_shared<httplib::Client>("http://api.ruckig.com");
}

nlohmann::json CloudClient::post(const nlohmann::json& params, bool throw_error) {
    const auto res = cli->Post("/calculate", params.dump(), "application/json");
    if (!res) {
        if (throw_error) {
            throw RuckigError("could not reach cloud API server");
        }

        std::cout << "[ruckig] could not reach cloud API server" << std::endl;
        return Result::Error;
    }
    if (res->status != 200) {
        if (throw_error) {
            throw RuckigError("could not reach cloud API server, error code: " + std::to_string(res->status) + " " + res->body);
        }

        std::cout << "[ruckig] could not reach cloud API server, error code: " << res->status << " " << res->body << std::endl;
        return Result::Error;
    }

    return nlohmann::json::parse(res->body);
}

} // namespace ruckig
