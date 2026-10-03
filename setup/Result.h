#pragma once

#include <string>
#include <utility>

namespace setup {

struct Result {
    bool ok = false;
    std::wstring message;
};

inline Result Success(std::wstring message) {
    return {true, std::move(message)};
}

inline Result Failure(std::wstring message) {
    return {false, std::move(message)};
}

}  // namespace setup
