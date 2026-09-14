# Copyright 2006 The QElectroTech Team
# This file is part of QElectroTech.
#
# QElectroTech is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# QElectroTech is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.

message(" - git_last_commit_sha")

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  #get GIT COMMIT SHA
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C ${QET_DIR} rev-parse --verify HEAD
    OUTPUT_VARIABLE  GIT_COMMIT_SHA
    RESULT_VARIABLE GIT_COMMIT_RESULT)

  # This strips terminating newline in the variable
  string(REGEX REPLACE "\n$" "" GIT_COMMIT_SHA "${GIT_COMMIT_SHA}")

  if(NOT GIT_COMMIT_RESULT EQUAL "0")
    message(
      FATAL_ERROR
      "git rev-parse --verify HEAD failed with "
      ${GIT_COMMIT_RESULT}
      ", please check")
  endif()
endif()

# Scope the definition to the three files that actually read it, instead of
# adding it to every compilation unit.
#
# As a global add_definitions() this value lands on the command line of every
# object in the tree, so the SHA changing -- which it does on every commit --
# invalidates the whole object tree and the compiler cache along with it, since
# the define is part of the cache key. A one-line commit then costs a full
# rebuild. Scoped this way, a commit recompiles these three files and relinks.
set_source_files_properties(
  ${QET_DIR}/sources/logging/crashhandler.cpp
  ${QET_DIR}/sources/logging/qetlogger.cpp
  ${QET_DIR}/sources/machine_info.cpp
  PROPERTIES COMPILE_DEFINITIONS GIT_COMMIT_SHA="${GIT_COMMIT_SHA}")
