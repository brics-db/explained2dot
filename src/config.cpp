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
 * config.cpp
 *
 *  Created on: 26.06.2018
 *      Author: Till Kolditz - Till.Kolditz@gmail.com
 */

#include "common.hpp"
#include "config.hpp"

namespace e2d {

    std::map<std::string, size_t> config_t::cmdIntArgs = {};
    std::map<std::string, std::string> config_t::cmdStrArgs = {};
    std::map<std::string, bool> config_t::cmdBoolArgs = { {"--help", false}, {"-h", false}, {"-?", false}, {"--exclude-mvc", false}, {"-m", false}, {"--compact", false}, {"-c", false}, {
            "--exclude-result", false}, {"-r", false}};
    std::map<std::string, typename config_t::cmdargtype_t> config_t::cmdArgTypes = { {"--help", argbool}, {"-h", argbool}, {"-?", argbool}, {"--exclude-mvc", argbool}, {"-m", argbool}, {"--compact",
            argbool}, {"-c", argbool}};

    config_t::config_t()
            : HELP(),
              EXCLUDE_MVC(),
              COMPACT(),
              EXCLUDE_RESULT() {
        update();
    }

    size_t config_t::parseint(
            const std::string& name,
            char** argv,
            int nArg) {
        if (argv == nullptr) {
            THROW_ERROR("Parameter \"" << name << "\" requires an additional integer value!", __LINE__);
        }
        std::string str(argv[nArg]);
        size_t idx = std::string::npos;
        size_t value;
        try {
            value = stoul(str, &idx);
        } catch (std::invalid_argument & exc) {
            THROW_ERROR2(exc, "Value for parameter \"" << name << "\" is not an integer (is \"" << str << "\")!", __LINE__)
        }
        if (idx < str.length()) {
            THROW_ERROR("Value for parameter \"" << name << "\" is not an integer (is \"" << str << "\")!", __LINE__)
        }
        cmdIntArgs[name] = value;
        return 1;
    }

    size_t config_t::parsestr(
            const std::string& name,
            char** argv,
            int nArg) {
        if (argv == nullptr) {
            THROW_ERROR("Parameter \"" << name << "\" requires an additional string value!", __LINE__)
        }
        cmdStrArgs[name] = argv[nArg];
        return 1;
    }

    size_t config_t::parsebool(
            const std::string& name) {
        cmdBoolArgs[name] = true;
        return 0;
    }

    size_t config_t::parsearg(
            const std::string& name,
            cmdargtype_t argtype,
            char** argv,
            int nArg) {
        switch (argtype) {
            case argint:
                return parseint(name, argv, nArg);

            case argstr:
                return parsestr(name, argv, nArg);

            case argbool:
                return parsebool(name);
        }
        return 0;
    }

    void config_t::init(
            int argc,
            char ** argv) {
        if (argc > 1) {
            for (int nArg = 1; nArg < argc; ++nArg) {
                if (argv[nArg][0] != '-') {
                    continue;
                }
                bool recognized = false;
                for (auto p : cmdArgTypes) {
                    if (p.first.compare(argv[nArg]) == 0) {
                        recognized = true;
                        if ((nArg + 1) >= argc) {
                            THROW_ERROR("Required value for parameter \"" << argv[nArg] << "\" missing!", __LINE__)
                        }
                        nArg += parsearg(p.first, p.second, nArg < argc ? argv : nullptr, nArg + 1);
                        break;
                    }
                }
                if (!recognized) {
                    THROW_ERROR("Parameter \"" << argv[nArg] << "\" is unknown!", __LINE__)
                }
            }
        }
        update();
    }

    void config_t::update() {
        EXCLUDE_MVC = cmdBoolArgs["--exclude-mvc"] | cmdBoolArgs["-m"];
        HELP = cmdBoolArgs["--help"] | cmdBoolArgs["-h"] | cmdBoolArgs["-?"];
        COMPACT = cmdBoolArgs["--compact"] | cmdBoolArgs["-c"];
        EXCLUDE_RESULT = cmdBoolArgs["--exclude-result"] | cmdBoolArgs["-r"];
    }

}
