// Copyright 2018 Till Kolditz
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * common.hpp
 *
 *  Created on: 26.06.2018
 *      Author: Till Kolditz - Till.Kolditz@gmail.com
 */

#pragma once

#include <sstream>
#include <exception>

namespace e2d {

#define THROW_ERROR(MESSAGE, LINE) {                                                \
    std::string filename(__FILE__);                                                 \
    auto filesub = filename.substr(filename.rfind('/') + 1);                        \
    std::stringstream ss;                                                           \
    ss << "[ERROR @ " << filesub << ':' << LINE << "] " << MESSAGE << std::endl;    \
    throw std::runtime_error(ss.str());                                             \
}

#define THROW_ERROR2(ROOT_EXC, MESSAGE, LINE) {                                     \
    std::string filename(__FILE__);                                                 \
    auto filesub = filename.substr(filename.rfind('/') + 1);                        \
    std::stringstream ss;                                                           \
    ss << "[ERROR @ " << filesub << ':' << LINE << "] " << MESSAGE << std::endl;    \
    auto rootExcMsg = ROOT_EXC.what();                                              \
    ss << "Root Exception: " << (rootExcMsg != nullptr ? rootExcMsg : "");          \
    throw std::runtime_error(ss.str());                                             \
}

#define PRINT_ERROR_ON(PREDICATE, MESSAGE, LINE)                                    \
if (PREDICATE) {                                                                    \
    std::string filename(__FILE__);                                                 \
    auto filesub = filename.substr(filename.rfind('/') + 1);                        \
    std::stringstream ss;                                                           \
    ss << "[ERROR @ " << filesub << ':' << LINE << "] " << MESSAGE << std::endl;    \
    std::cerr << ss.str();                                                          \
    return LINE;                                                                    \
}

#define PRINT_WARN_ON(PREDICATE, MESSAGE, LINE)                                     \
if (PREDICATE) {                                                                    \
    std::string filename(__FILE__);                                                 \
    auto filesub = filename.substr(filename.rfind('/') + 1);                        \
    std::stringstream ss;                                                           \
    ss << "[WARN @ " << filesub << ':' << LINE << "] " << MESSAGE << std::endl;     \
    std::cerr << ss.str();                                                          \
    return LINE;                                                                    \
}

}
