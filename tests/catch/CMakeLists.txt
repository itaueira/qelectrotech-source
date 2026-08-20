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

cmake_minimum_required(VERSION 3.5)

message("..___________________________________________________________________")

project(C_unittests LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

SET(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(".. PROJECT_NAME              :" ${PROJECT_NAME})
message(".. PROJECT_SOURCE_DIR        :" ${PROJECT_SOURCE_DIR})
if(NOT DEFINED QET_DIR)
  set(QET_DIR "../..")
  message(".. QET_DIR is not set, assuming QET is ../..")
endif()
message(".. QET_DIR                   :" ${QET_DIR})
if(NOT DEFINED QET_COMPONENTS)
  message(".. QET_COMPONENTS is not set !!! I set them up !!!")
  include(../../cmake/qet_compilation_vars.cmake)
endif()
if(NOT DEFINED QT_VERSION_MAJOR)
  find_package(
    QT
   NAMES
    Qt6
    Qt5
   COMPONENTS
    ${QET_COMPONENTS}
   REQUIRED
   )
endif()
message(".. QT_VERSION_MAJOR   :" ${QT_VERSION_MAJOR})

find_package(
  Qt${QT_VERSION_MAJOR}
 COMPONENTS
 ${QET_COMPONENTS}
 REQUIRED)

# Qt6-only: the tests link ${QET_PRIVATE_LIBRARIES}, which contains
# Qt::GuiPrivate; Qt6 only creates that target when the component is
# explicitly requested (Qt5 creates it implicitly and has no such package).
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS GuiPrivate)
endif()

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.10)

FetchContent_MakeAvailable(Catch2)

include(../../cmake/fetch_kdeaddons.cmake)
include(../../cmake/fetch_singleapplication.cmake)
include(../../cmake/fetch_pugixml.cmake)

enable_testing()

add_executable(
    ${PROJECT_NAME}
    tst_My_test.cpp
    src/borderproperties_test.cpp
    src/catalog_test.cpp
    src/catalogassignment_test.cpp
    src/catalogimport_test.cpp
    src/environment_test.cpp
    src/iecstructure_test.cpp
    src/renumber_test.cpp
    main.cpp
    ${QET_DIR}/sources/borderproperties.cpp
    ${QET_DIR}/sources/borderproperties.h
    # The numbering rules and the IEC structure have the same property as the
    # catalog core: no Element, no Diagram, so the ordering and the inheritance
    # can be tested without a project open. ProjectRenumberer is the part that
    # does need one, and it is deliberately not here.
    ${QET_DIR}/sources/autoNum/iecstructure.cpp
    ${QET_DIR}/sources/autoNum/iecstructure.h
    ${QET_DIR}/sources/autoNum/numberingformat.cpp
    ${QET_DIR}/sources/autoNum/numberingformat.h
    ${QET_DIR}/sources/autoNum/renumberplan.cpp
    ${QET_DIR}/sources/autoNum/renumberplan.h
    # The catalog core depends on Qt Core and Qt Sql only - not on QETApp,
    # not on Diagram - which is what lets the test binary link it with the
    # handful of sources below instead of half the program. Keep it that way:
    # the day the core needs an Element, these tests stop being cheap.
    ${QET_DIR}/sources/catalog/catalog.cpp
    ${QET_DIR}/sources/catalog/catalog.h
    ${QET_DIR}/sources/catalog/catalogassignment.cpp
    ${QET_DIR}/sources/catalog/catalogassignment.h
    ${QET_DIR}/sources/catalog/catalogclass.cpp
    ${QET_DIR}/sources/catalog/catalogclass.h
    ${QET_DIR}/sources/catalog/catalogimport.cpp
    ${QET_DIR}/sources/catalog/catalogimport.h
    ${QET_DIR}/sources/catalog/catalogpackage.cpp
    ${QET_DIR}/sources/catalog/catalogpackage.h
    ${QET_DIR}/sources/catalog/catalogpart.cpp
    ${QET_DIR}/sources/catalog/catalogpart.h
    ${QET_DIR}/sources/catalog/catalogproperty.cpp
    ${QET_DIR}/sources/catalog/catalogproperty.h
    ${QET_DIR}/sources/catalog/catalogrepository.cpp
    ${QET_DIR}/sources/catalog/catalogrepository.h
    ${QET_DIR}/sources/catalog/catalogschema.cpp
    ${QET_DIR}/sources/catalog/catalogschema.h
    ${QET_DIR}/sources/catalog/catalogtable.cpp
    ${QET_DIR}/sources/catalog/catalogtable.h
    ${QET_DIR}/sources/catalog/catalogtablereader.cpp
    ${QET_DIR}/sources/catalog/catalogtablereader.h
    # The environment and the in-use lock have the same property as the
    # catalog core: no QETApp, no Diagram, so the rules that decide
    # whether a project gets overwritten can be tested cheaply.
    ${QET_DIR}/sources/environment/filediskstate.h
    ${QET_DIR}/sources/environment/projectlock.cpp
    ${QET_DIR}/sources/environment/projectlock.h
    ${QET_DIR}/sources/environment/qetenvironment.cpp
    ${QET_DIR}/sources/environment/qetenvironment.h
    )

if(NOT BUILD_WITH_KF)
    target_sources(
        ${PROJECT_NAME}
        PRIVATE
        src/kautosavefile_test.cpp
        ${QET_DIR}/sources/ui/nokde/kautosavefile.cpp
        )
    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${QET_DIR}/sources/ui/nokde
        )
endif()

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    Catch2::Catch2
    PRIVATE
    ${KF_PRIVATE_LIBRARIES}
    ${QET_PRIVATE_LIBRARIES})

if (DEFINED ENV{CATCH_INCLUDE_DIR})
    set(CATCH_INCLUDE_DIR $ENV{CATCH_INCLUDE_DIR})
else ()
    set(CATCH_INCLUDE_DIR "") # set by Qt Creator wizard
endif ()
if (CATCH_INCLUDE_DIR STREQUAL "")
    message(".. CATCH_INCLUDE_DIR is not set,"
      "assuming Catch2 can be found automatically in your system")
elseif (EXISTS ${CATCH_INCLUDE_DIR})
    include_directories(${CATCH_INCLUDE_DIR})
endif ()
