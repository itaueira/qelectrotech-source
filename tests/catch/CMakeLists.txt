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
#
# Asked with QUIET and checked by target, which is what the root CMakeLists
# already does for the same question -- and the reason the root configured
# fine on a runner where this line did not.  Some distributions (Ubuntu
# 24.04 among them) create the Qt6::GuiPrivate target alongside Gui without
# shipping a separate Qt6GuiPrivateConfig.cmake, so REQUIRED on the package
# fails while the target the tests actually link is right there.  The target
# is still mandatory below; only the way of asking changed.
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS GuiPrivate)
  if(NOT TARGET Qt${QT_VERSION_MAJOR}::GuiPrivate)
    message(FATAL_ERROR
      "Qt${QT_VERSION_MAJOR}::GuiPrivate was not found, and the test targets "
      "link it through QET_PRIVATE_LIBRARIES. Install the Qt6 private headers "
      "(e.g. 'qt6-base-private-dev' on Debian/Ubuntu) or use a Qt build that "
      "provides the GuiPrivate component.")
  endif()
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
    src/titleblockvariables_test.cpp
    src/titleblockauthoring_test.cpp
    src/environment_test.cpp
    src/symbol_test.cpp
    src/pinout_test.cpp
    src/symbolpreview_test.cpp
    src/assemblystate_test.cpp
    src/iecstructure_test.cpp
    src/renumber_test.cpp
    src/connectornumbering_test.cpp
    src/macro_test.cpp
    src/circuit_test.cpp
    src/ioassignment_test.cpp
    src/iocircuit_test.cpp
    src/iocommon_test.cpp
    src/io_test.cpp
    src/iosheet_test.cpp
    src/iotree_test.cpp
    src/location_test.cpp
    src/locationcontainment_test.cpp
    src/enclosuretransfer_test.cpp
    src/mountinglayout_test.cpp
    src/mountingcheck_test.cpp
    src/mountingmeasure_test.cpp
    src/mountingpartview_test.cpp
    src/locationboundary_test.cpp
    src/locationvariable_test.cpp
    src/terminalstripstructure_test.cpp
    src/collectionloadgate_test.cpp
    src/collectionthreadbudget_test.cpp
    src/updatecoalescer_test.cpp
    src/componentlabel_test.cpp
    src/drcrule_test.cpp
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
    ${QET_DIR}/sources/autoNum/assemblystate.cpp
    ${QET_DIR}/sources/autoNum/assemblystate.h
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
    ${QET_DIR}/sources/macro/circuitclipboard.cpp
    ${QET_DIR}/sources/macro/circuitclipboard.h
    ${QET_DIR}/sources/macro/circuitlayout.cpp
    ${QET_DIR}/sources/macro/circuitlayout.h
    ${QET_DIR}/sources/macro/circuittable.cpp
    ${QET_DIR}/sources/macro/circuittable.h
    ${QET_DIR}/sources/macro/macroparameter.cpp
    ${QET_DIR}/sources/macro/macroparameter.h
    ${QET_DIR}/sources/macro/macroparameterset.cpp
    ${QET_DIR}/sources/macro/macroparameterset.h
    ${QET_DIR}/sources/macro/macrosequence.cpp
    ${QET_DIR}/sources/macro/macrosequence.h
    ${QET_DIR}/sources/macro/macrosubstitution.cpp
    ${QET_DIR}/sources/macro/macrosubstitution.h
    ${QET_DIR}/sources/macro/macrouuid.cpp
    ${QET_DIR}/sources/macro/macrouuid.h
    # A lista de E/S tem a mesma propriedade do núcleo do catálogo e do
    # modelo de macro: QDomDocument e QString, nada de Diagram nem de QETApp.
    # O `elementdata.h` entra só como cabeçalho, pelo enum PlcIOType — o `.cpp`
    # dele arrastaria `qetxml`, `nameslist` e o QETApp inteiro atrás, e é
    # justamente isso que a serialização própria do IoPoint evita.
    ${QET_DIR}/sources/plc/ioassignment.cpp
    ${QET_DIR}/sources/plc/ioassignment.h
    ${QET_DIR}/sources/plc/iocircuit.cpp
    ${QET_DIR}/sources/plc/iocircuit.h
    ${QET_DIR}/sources/plc/iocommon.cpp
    ${QET_DIR}/sources/plc/iocommon.h
    ${QET_DIR}/sources/plc/iolist.cpp
    ${QET_DIR}/sources/plc/iolist.h
    ${QET_DIR}/sources/plc/iopoint.cpp
    ${QET_DIR}/sources/plc/iopoint.h
    ${QET_DIR}/sources/plc/iosheet.cpp
    ${QET_DIR}/sources/plc/iosheet.h
    ${QET_DIR}/sources/plc/iotree.cpp
    ${QET_DIR}/sources/plc/iotree.h
    # A arvore de localizacoes nao conhece nada alem de QString e QDomDocument,
    # de proposito: ela e aritmetica de caminho, e a aritmetica e o que
    # precisa de bancada. O que a liga a folha fica em sources/undocommand
    # e em sources/ui, fora daqui.
    ${QET_DIR}/sources/location/locationtree.cpp
    ${QET_DIR}/sources/location/locationtree.h
    ${QET_DIR}/sources/location/projectlocation.cpp
    ${QET_DIR}/sources/location/projectlocation.h
    # The containment rule knows nothing but QRectF and QString, which is
    # what lets CU-32.2 - six push buttons, one dragged out, one dragged
    # in - be proved here instead of on a screen. What draws the rectangle
    # and what pushes the undo command stay out, in sources/qetgraphicsitem
    # and sources/undocommand.
    ${QET_DIR}/sources/location/locationcontainment.cpp
    ${QET_DIR}/sources/location/locationcontainment.h
    # The external wire rule is two strings and an answer: whether a
    # conductor between two locations leaves the panel. It is here and not on
    # a folio because what can be got wrong is the comparison - QCM1 against
    # QCM10, the door of an enclosure against its own body - and none of that
    # needs a conductor drawn. The painting override that consumes it stays
    # out, in sources/qetgraphicsitem.
    ${QET_DIR}/sources/location/locationboundary.cpp
    ${QET_DIR}/sources/location/locationboundary.h
    # The enclosure transfer rule is arithmetic over rectangles: it says
    # which mounted positions survive a change of enclosure and names the
    # ones that do not. No scene, no project, no database - so the half of
    # the acceptance criterion that matters (nothing is dropped silently)
    # is proved here, and only the moving of items on screen waits for the
    # mounting plate editor.
    ${QET_DIR}/sources/location/enclosuretransfer.cpp
    ${QET_DIR}/sources/location/enclosuretransfer.h
    # What is mounted on each face of each enclosure, in millimetre. It is
    # the caller the rule above never had, and it is here for the same
    # reason the rule is: a layout is a list of rectangles and a pair of
    # numbers, so a plate can be filled, written to a file, read back and
    # swapped for a smaller plate without a window ever opening. What draws
    # the face and what pushes the undo command stay out.
    ${QET_DIR}/sources/location/mountinglayout.cpp
    ${QET_DIR}/sources/location/mountinglayout.h
    # The three questions a mounting surface is asked: do two parts want the
    # same room, does each part have the air it asked for, and is everything
    # on the plate at all. Rectangles and a hash of clearances, so that "these
    # two collide" and "this drive is 70 mm short" are settled here instead of
    # on a screen - a geometry rule that can only be exercised by dragging
    # something is a rule nobody checks twice. What highlights the offending
    # part and what lists it in a panel stay out, in the layout editor.
    ${QET_DIR}/sources/location/mountingcheck.cpp
    ${QET_DIR}/sources/location/mountingcheck.h
    # The dimension arithmetic: what a dimension between two points reads,
    # where a hole sits on the mounting surface, and what movement makes a
    # dimension read a typed value. It is QPointF and qreal and nothing
    # else, which is what lets the sign of y - down, as the mounting frame
    # declares it - be nailed here rather than found in a workshop. What
    # draws the dimension and what pushes the undo command stay out, in
    # sources/qetgraphicsitem and sources/undocommand.
    ${QET_DIR}/sources/location/mountingmeasure.cpp
    ${QET_DIR}/sources/location/mountingmeasure.h
    # The road from a product code to the body of the part: how big it is, how
    # much air it wants, where its axis is. It is the one file of the layout
    # that opens the catalog, and it is here because both of its ends already
    # are - the catalog core below and the mounting rules above - so a
    # clearance can travel from a data base row to a named violation without a
    # window opening. It is also the only place where "nobody measured this"
    # and "this is zero" are still told apart, which is why it has to be
    # exercised against a real catalog and not against a hash somebody typed.
    ${QET_DIR}/sources/location/mountingpartview.cpp
    ${QET_DIR}/sources/location/mountingpartview.h
    # When a project database change is announced. As pure as the collection
    # gate rule: a counter and a flag, no Qt at all. What emits the signal is
    # projectDataBase, which needs an open project and therefore lives in
    # C_uitests, in dbnotify_test.cpp.
    ${QET_DIR}/sources/dataBase/updatecoalescer.cpp
    ${QET_DIR}/sources/dataBase/updatecoalescer.h
    # What a design rule declares about itself, and the register that holds
    # the rules. QString and QHash, and QCoreApplication only for the word
    # the panel shows: no project, no data base, no folio - which is what
    # lets the severity order, the tolerant read of the project file and
    # the refusal of a repeated identifier be nailed before a single rule
    # is written against the contract. The two engines that run the rules
    # need a QETProject and stay out, in C_uitests.
    ${QET_DIR}/sources/drc/drcrule.cpp
    ${QET_DIR}/sources/drc/drcrule.h
    ${QET_DIR}/sources/drc/drcruleset.cpp
    ${QET_DIR}/sources/drc/drcruleset.h
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
    # E o gerador, que e a aritmetica dos dois aplicada a uma pinagem: onde
    # cada borne cai, quantos blocos a peca ocupa e o que o simbolo produzido
    # tem escrito. Nao abre janela nem toca no repositorio, entao cada bloco
    # gerado pode ser conferido borne a borne.
    ${QET_DIR}/sources/ElementsCollection/pinoutgenerator.cpp
    ${QET_DIR}/sources/ElementsCollection/pinoutgenerator.h
    # E a regra do terminal repetido, que é comparação de nome: qual borne já
    # está desenhado em qual componente, e a frase que se diz a quem tentou
    # desenhá-lo de novo. O PinoutUsageScanner, que lê isso de um projeto
    # aberto, fica de fora de propósito — mesma razão do SheetSymbolExtractor.
    ${QET_DIR}/sources/ElementsCollection/pinoutusage.cpp
    ${QET_DIR}/sources/ElementsCollection/pinoutusage.h
    # Quando a coleção de símbolos pode começar a carregar (T42). É a regra
    # mais pura que existe aqui: nem Qt ela inclui — só um contador e dois
    # sinalizadores —, e é justamente por não depender de janela que ela pode
    # ser conferida sem uma. O agendamento em si fica no QETDiagramEditor, que
    # precisa de QETApp e por isso não entra em suíte nenhuma.
    ${QET_DIR}/sources/ElementsCollection/collectionloadgate.cpp
    ${QET_DIR}/sources/ElementsCollection/collectionloadgate.h
    # Quantos núcleos a varredura da coleção pode tomar (T42, passo 2). Regra
    # pura outra vez: entra o número de núcleos da máquina, sai o número de
    # threads. **O que ela não vê é a fiação** — quem chama o
    # setMaxThreadCount() com o número que ela devolve é o
    # ElementsCollectionModel, que precisa de QETApp e não entra em suíte
    # nenhuma. Apagar aquela chamada deixa esta suíte inteira verde; foi
    # plantado e conferido. Só medição pega.
    ${QET_DIR}/sources/ElementsCollection/collectionthreadbudget.cpp
    ${QET_DIR}/sources/ElementsCollection/collectionthreadbudget.h
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
    # A régua de bornes vista como estrutura, e a regra que decide o que o
    # programa pode pontear sozinho. Depende de QString, de QVector e do enum
    # de tipo de borne — o `elementdata.h` entra só como cabeçalho, pelo mesmo
    # motivo e do mesmo jeito que na lista de E/S acima. Nem Element nem
    # RealTerminal, de propósito: é essa ausência que permite conferir por
    # contagem o que de outro modo só se veria montando uma régua e olhando
    # para ela. O que liga a regra ao TerminalStrip fica de fora, em
    # sources/TerminalStrip e em sources/undocommand.
    ${QET_DIR}/sources/TerminalStrip/terminalstripstructure.cpp
    ${QET_DIR}/sources/TerminalStrip/terminalstripstructure.h
    # O que uma etiqueta de componente pergunta ao banco do projeto, e o que
    # ela faz da resposta (T21). Duas strings de consulta e duas
    # transformações de valor: nem QSqlQuery entra aqui, o que é justamente
    # o que permite ler as perguntas e conferir as respostas sem projeto
    # aberto. **O que este alvo não vê é a fiação** — quem executa as duas
    # consultas e quem carrega a revisão do folio para as entradas é o
    # ComponentLabelCollector, que precisa de QETProject e por isso mora na
    # outra suíte. Apagar aquela chamada, ou apontar a consulta para a vista
    # filtrada, deixa esta suíte inteira verde; as duas coisas foram
    # plantadas e conferidas, e só os casos de src/ui/ as pegaram.
    ${QET_DIR}/sources/label/componentlabelquery.cpp
    ${QET_DIR}/sources/label/componentlabelquery.h
    ${QET_DIR}/sources/label/labelentry.h
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
# cabecalho e biblioteca vem do upstream, e e assim de proposito.
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

# Optional hook for test sources that live outside this repository. A fork can drop its own
# tests.cmake into tests/catch/local/ to add sources and definitions to the target above.
# Nothing here requires it: when the file is absent, the target builds exactly as before.
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/local/tests.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/local/tests.cmake)
endif ()

# ---------------------------------------------------------------------------
# C_uitests - the second suite: the one that needs a project open.
#
# It is a separate executable on purpose. C_unittests lists its sources by
# hand, and that hand-written list is a contract: it records that those rules
# can be computed without a sheet, an element or a project. Linking it against
# qet_core would erase the contract - everything would compile, and the day a
# pure rule started needing an Element nobody would notice.
#
# So the split is: C_unittests stays as it is, and anything that has to open a
# real project lands here, where qet_core brings the whole program in.
#
# Guarded by if(TARGET qet_core) because this directory can also be configured
# on its own, from tests/catch/, with no parent project - which is what the
# QET_DIR fallback above exists for. In that mode the target does not exist
# and only C_unittests is built.
if(TARGET qet_core)
    add_executable(
        C_uitests
        main.cpp
        src/ui/uibench.cpp
        src/ui/uibench.h
        # Reads a PDF the export has just written. It is a bench helper and
        # not a test, for the same reason uibench is: what it answers - is
        # this word findable, does this jump land on this page - is a
        # property of the emitted bytes, and more than one case of the
        # T37 needs the same answer.
        src/ui/pdfprobe.cpp
        src/ui/pdfprobe.h
        src/ui/pdfexport_test.cpp
        src/ui/pdffolios_test.cpp
        src/ui/projectopen_test.cpp
        src/ui/appearance_test.cpp
        src/ui/dragassign_test.cpp
        src/ui/iecoff_test.cpp
        src/ui/assignpart_test.cpp
        src/ui/contextmenuassign_test.cpp
        src/ui/renumberproject_test.cpp
        src/ui/assemblymark_test.cpp
        src/ui/createsymbol_test.cpp
        src/ui/cable_test.cpp
        src/ui/dashedwire_test.cpp
        src/ui/ioimport_test.cpp
        src/ui/ioassign_test.cpp
        src/ui/macroinsert_test.cpp
        src/ui/backup_test.cpp
        src/ui/crashrecovery_test.cpp
        src/ui/listengine_test.cpp
        src/ui/titleblockeditor_test.cpp
        src/ui/navigate_test.cpp
        src/ui/dbnotify_test.cpp
        src/ui/componentlabel_test.cpp
        src/ui/mountinglayoutsave_test.cpp
        src/ui/connectorlink_test.cpp
        src/ui/connectorrenumber_test.cpp)

    target_link_libraries(
        C_uitests
        PUBLIC
        Catch2::Catch2
        PRIVATE
        qet_core
        Qt::Test)

    # ${QET_DIR} for the same reason as above: diagramcontext.h includes
    # "pugixml/src/pugixml.hpp", a path that only resolves from the root.
    # The current directory is what lets a test in src/ui/ include "uibench.h".
    target_include_directories(
        C_uitests
        PRIVATE
        ${QET_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR})

    # The fixtures are the projects the program ships with. Passing the path in
    # rather than deriving it from the working directory means the suite gives
    # the same answer whether it is run from the build tree, from the source
    # tree or by the CI.
    target_compile_definitions(
        C_uitests
        PRIVATE
        QET_EXAMPLES_DIR="${QET_DIR}/examples")
endif()
