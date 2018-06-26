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
 * config.hpp
 *
 *  Created on: 26.06.2018
 *      Author: Till Kolditz - Till.Kolditz@gmail.com
 */

#pragma once

#include <cstddef>
#include <string>
#include <map>

namespace e2d {

    class config_t {

        enum cmdargtype_t {
            argint,
            argstr,
            argbool
        };

        static std::map<std::string, size_t> cmdIntArgs;

        static std::map<std::string, std::string> cmdStrArgs;

        static std::map<std::string, bool> cmdBoolArgs;

        static std::map<std::string, cmdargtype_t> cmdArgTypes;

        size_t parseint(
                const std::string& name,
                char** argv,
                int nArg);

        size_t parsestr(
                const std::string& name,
                char** argv,
                int nArg);

        size_t parsebool(
                const std::string& name);

        size_t parsearg(
                const std::string& name,
                cmdargtype_t argtype,
                char** argv,
                int nArg);

    public:
        bool DO_EXCLUDE_MVC;
        bool HELP;

        config_t();

        void init(
                int argc,
                char ** argv);
    };

}
