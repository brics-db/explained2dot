# Copyright (c) 2017 Till Kolditz
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CMAKE_MINIMUM_REQUIRED(VERSION 3.5 FATAL_ERROR)

IF(POLICY CMP0048)
    CMAKE_POLICY(SET CMP0048 NEW)
    PROJECT(explained2dot VERSION 1.0 LANGUAGES CXX)
ELSE()
    PROJECT(explained2dot LANGUAGES CXX)
ENDIF()

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
   message(FATAL_ERROR "In-source builds are not allowed.")
endif("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")

#FIND_PACKAGE(Boost REQUIRED COMPONENTS SYSTEM FILESYSTEM)
FIND_PACKAGE(Boost REQUIRED)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Wall -Wextra -pedantic -Weffc++ -fexceptions -fnon-call-exceptions")

ADD_EXECUTABLE(explained2dot src/explained2dot.cpp)
SET_PROPERTY(TARGET explained2dot PROPERTY CXX_STANDARD 14)
SET_PROPERTY(TARGET explained2dot PROPERTY CXX_STANDARD_REQUIRED ON)
TARGET_INCLUDE_DIRECTORIES(explained2dot PUBLIC Boost_INCLUDE_DIR)
#TARGET_LINK_LIBRARIES(explained2dot ${Boost_SYSTEM_LIBRARY_RELEASE} ${Boost_FILESYSTEM_LIBRARY_RELEASE} ${Boost_FILESYSTEM_LIBRARY_RELEASE})
TARGET_LINK_LIBRARIES(explained2dot boost_system boost_filesystem boost_iostreams)

