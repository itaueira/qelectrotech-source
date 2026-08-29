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

# Qt Test, e só no alvo de teste: ele traz o QTest e o QSignalSpy, que são o que
# permite construir um widget, clicar nele e afirmar sobre o resultado — sem
# nada disso aparecer na tela, porque o main.cpp força a plataforma "offscreen".
# O programa não linka isto, e não deve.
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

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
    src/catalogclasspackage_test.cpp
    src/catalogimport_test.cpp
    src/bancada_conversao_test.cpp
    src/titleblockvariables_test.cpp
    src/environment_test.cpp
    src/symbol_test.cpp
    src/pinout_test.cpp
    src/symbolpreview_test.cpp
    src/iecstructure_test.cpp
    src/renumber_test.cpp
    src/macro_test.cpp
    main.cpp
    ${QET_DIR}/sources/borderproperties.cpp
    ${QET_DIR}/sources/borderproperties.h
    # O saco de chave/valor que o carimbo, o folio e o componente usam para
    # guardar informação. Entra para que a leitura da estrutura IEC possa ser
    # testada contra a DiagramContext DE VERDADE, e não contra um dublê que
    # continuaria passando depois de a classe entregue mudar.
    #
    # Ele arrasta três arquivos atrás — diagramcontext usa QET::findInDomElement,
    # e qet.cpp usa QET::Icons e o ShortcutManager. Conferido um por um: nenhum
    # dos quatro referencia QETApp, Element, Diagram ou QETProject, então a
    # regra que os comentários abaixo protegem continua valendo. Só de Qt eles
    # dependem, e Qt::Widgets já estava ligado.
    ${QET_DIR}/sources/diagramcontext.cpp
    ${QET_DIR}/sources/diagramcontext.h
    ${QET_DIR}/sources/qet.cpp
    ${QET_DIR}/sources/qet.h
    ${QET_DIR}/sources/qeticons.cpp
    ${QET_DIR}/sources/qeticons.h
    ${QET_DIR}/sources/shortcutmanager.cpp
    ${QET_DIR}/sources/shortcutmanager.h
    # O vocabulário de variáveis do carimbo, e a limpeza do que ninguém
    # preencheu. Entra aqui porque depende só de QObject e QHash: dá para
    # testar sem arrastar o carimbo inteiro, que arrastaria o programa atrás.
    ${QET_DIR}/sources/qetinformation.cpp
    ${QET_DIR}/sources/qetinformation.h
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
    # O modelo e o motor do macro parametrizado tem a mesma propriedade
    # do nucleo do catalogo: QDomDocument e QString, nada de Diagram nem
    # de QETApp. A parte que precisa de um projeto aberto e o
    # DiagramEventAddMacro, e ele esta deliberadamente fora daqui.
    ${QET_DIR}/sources/macro/macroparameter.cpp
    ${QET_DIR}/sources/macro/macroparameter.h
    ${QET_DIR}/sources/macro/macroparameterset.cpp
    ${QET_DIR}/sources/macro/macroparameterset.h
    ${QET_DIR}/sources/macro/macrosequence.cpp
    ${QET_DIR}/sources/macro/macrosequence.h
    ${QET_DIR}/sources/macro/macrosubstitution.cpp
    ${QET_DIR}/sources/macro/macrosubstitution.h
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
    ${QET_DIR}/sources/catalog/catalogclasspackage.cpp
    ${QET_DIR}/sources/catalog/catalogclasspackage.h
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
    # The symbol being built out of the drawing is geometry, validation and
    # XML: no scene, no item, no window. SheetSymbolExtractor is the part that
    # does touch the graphics items, and it is deliberately not here — it is
    # kept thin for exactly that reason.
    ${QET_DIR}/sources/ElementsCollection/symbolbuilder.cpp
    ${QET_DIR}/sources/ElementsCollection/symbolbuilder.h
    ${QET_DIR}/sources/ElementsCollection/symbolgroup.cpp
    ${QET_DIR}/sources/ElementsCollection/symbolgroup.h
    # O modelo de bloco e a convenção de lados são dado e aritmética de
    # grade: onde cada borne cai, quantos blocos a pinagem ocupa. Nada disso
    # abre janela, então cada conta pode ser conferida por número.
    ${QET_DIR}/sources/ElementsCollection/pinoutblocktemplate.cpp
    ${QET_DIR}/sources/ElementsCollection/pinoutblocktemplate.h
    # A pré-visualização do símbolo é um widget, e entra aqui porque widget dá
    # para testar sem tela: ver o QT_QPA_PLATFORM=offscreen em main.cpp. Ela
    # não depende de item gráfico nenhum: a leitura da caneta a partir da string
    # de estilo mora no SymbolShape, que é onde a string mora.
    ${QET_DIR}/sources/ElementsCollection/ui/symbolpreview.cpp
    ${QET_DIR}/sources/ElementsCollection/ui/symbolpreview.h
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

# A raiz do repositorio no caminho de inclusao. O programa a ganha de graca
# (CMAKE_INCLUDE_CURRENT_DIR ON na raiz, CMakeLists.txt:60); este alvo mora em
# tests/catch/ e por isso nao. Precisa dela porque diagramcontext.h inclui
# "pugixml/src/pugixml.hpp", caminho que so resolve a partir da raiz.
#
# Isto escolhe de proposito o cabecalho do submodulo (1.16) e nao o da copia que
# o FetchContent baixa (1.15), porque e assim que o programa compila. Teste que
# compila diferente do programa testa outra coisa. A diferenca de versao entre
# cabecalho e biblioteca vem do upstream e esta registrada em todo/BUILD.md.
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${QET_DIR})

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    Catch2::Catch2
    PRIVATE
    ${KF_PRIVATE_LIBRARIES}
    ${QET_PRIVATE_LIBRARIES}
    pugixml::pugixml
    Qt::Test)

# Onde estão os arquivos de exemplo que o teste importa de verdade. Existe para que
# o teste prove o arquivo QUE É ENTREGUE, e não uma cópia dele escrita no próprio
# teste: uma cópia continuaria passando depois de o arquivo entregue quebrar.
get_filename_component(QET_ABS_DIR "${QET_DIR}" ABSOLUTE)
target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE
    QET_TEST_DATA_DIR="${QET_ABS_DIR}/todo/exemplos")

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
