#pragma once

#include <string>

namespace celia {

std::string json_string(const std::string& value);
std::string first_json_string_argument(const std::string& request);

} // namespace celia
