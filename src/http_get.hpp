#pragma once

#include <stdexcept>
#include <string>

namespace tmf {

struct UriError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct IoError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct HttpGetResult {
    int status = 0;
    std::string body;
};

HttpGetResult http_get(const std::string& url);

}  // namespace tmf
