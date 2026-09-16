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
    src/catalogvalueorigin_test.cpp
    src/physicalview_test.cpp
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
    src/bommeasure_test.cpp
    src/bomcollector_test.cpp
    src/locationcontainment_test.cpp
    src/enclosuretransfer_test.cpp
    src/mountinglayout_test.cpp
    src/mountingcheck_test.cpp
    src/mountingmeasure_test.cpp
    src/drillingtable_test.cpp
    src/drillingorigin_test.cpp
    src/mountingpartview_test.cpp
    src/mountingprofile_test.cpp
    src/mountingclip_test.cpp
    src/mountingalign_test.cpp
    src/locationboundary_test.cpp
    src/locationvariable_test.cpp
    src/option_test.cpp
    src/terminalstripstructure_test.cpp
    src/terminalmove_test.cpp
    src/collectionloadgate_test.cpp
    src/collectionthreadbudget_test.cpp
    src/updatecoalescer_test.cpp
    src/componentlabel_test.cpp
    src/drcrule_test.cpp
    src/drcfinding_test.cpp
    src/connectorways_test.cpp
    src/connectorswap_test.cpp
    src/csvwriter_test.cpp
    src/bomquery_test.cpp
    src/foliovariables_test.cpp
    src/projectrevision_test.cpp
    src/apppreferences_test.cpp
    main.cpp
    ${QET_DIR}/sources/borderproperties.cpp
    ${QET_DIR}/sources/borderproperties.h
    # A substituição que transforma o modelo de folio ("%id/%total") no número
    # de página que se lê. Ela depende só de QString, e é por isso que cabe
    # aqui: o dono dela seria o BorderTitleBlock, que é um QGraphicsItem e
    # arrastaria o renderizador de carimbo e o programa atrás. O segundo leitor
    # — a lista de fiação exportada — não tem cena nenhuma para abrir, e foi
    # exatamente por não poder chamar o carimbo que ele imprimia o modelo.
    ${QET_DIR}/sources/foliovariables.cpp
    ${QET_DIR}/sources/foliovariables.h
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
    # How a line of material arrives at its number: counted by the piece,
    # or measured by the length and cut to size. A rule over a string and a
    # double, no catalog and no project, which is what lets "a count never
    # prints 14.0" and "two cut pieces of one rail are one line" be nailed
    # here. Who fills the length in is the layout, and it stays out.
    ${QET_DIR}/sources/location/bommeasure.cpp
    ${QET_DIR}/sources/location/bommeasure.h
    # As duas metades do que um quadro custa, somadas numa lista so: o
    # componente desenhado e o involucro que ninguem desenha. E regra pura --
    # listas de struct entrando, linhas somadas e pendencias contadas saindo,
    # sem banco e sem projeto --, e e por isso que cabe aqui: a falha que ela
    # existe para impedir e silenciosa, uma planilha de compra sem armario que
    # nao tem nada de errado na cara. Quem executa a consulta e escreve o
    # arquivo precisa de projeto aberto e fica fora.
    ${QET_DIR}/sources/location/bomcollector.cpp
    ${QET_DIR}/sources/location/bomcollector.h
    # A arvore de localizacoes nao conhece nada alem de QString e QDomDocument,
    # de proposito: ela e aritmetica de caminho, e a aritmetica e o que
    # precisa de bancada. O que a liga a folha fica em sources/undocommand
    # e em sources/ui, fora daqui.
    ${QET_DIR}/sources/location/locationtree.cpp
    ${QET_DIR}/sources/location/locationtree.h
    ${QET_DIR}/sources/location/projectlocation.cpp
    ${QET_DIR}/sources/location/projectlocation.h
    # A árvore de opções está aqui pela mesma razão que a de localizações:
    # ela é aritmética de QString e de QDomDocument, e nada mais. Nem folha,
    # nem elemento, nem projeto — o que é justamente o que permite provar
    # sem tela que ligar uma opção não é o mesmo que ela valer, e que um
    # arquivo estragado (pai que não existe, laço, dois nomes iguais) é
    # consertado na leitura em vez de recusado. O que liga a árvore ao
    # projeto são duas linhas do QETProject, e essas ficam de fora daqui:
    # estão provadas em src/ui/projectoptions_test.cpp, do C_uitests.
    ${QET_DIR}/sources/options/optiontree.cpp
    ${QET_DIR}/sources/options/optiontree.h
    ${QET_DIR}/sources/options/projectoption.cpp
    ${QET_DIR}/sources/options/projectoption.h
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
    # What the workshop is handed to drill from: where each hole is, how big,
    # what for, and whose. A record and the delimited text it goes out as -
    # no sheet metal drawn, no project open - which is what lets the defect
    # this file exists against be pinned before there is a drilling view to
    # produce one. That defect is silent: a destination holding the
    # separator shifts every column of its row, and the shifted row opens in
    # a spreadsheet looking exactly as well formed as a right one. The
    # quoting itself is not here, it is csvwriter below; what is here is
    # that this table goes through it.
    ${QET_DIR}/sources/location/drillingtable.cpp
    ${QET_DIR}/sources/location/drillingtable.h
    # Which corner of the plate the workshop measures from, and the one place
    # a position becomes the number printed beside a hole. The whole of it is
    # four corners, a shift and two inverse functions - no plate drawn, no
    # project open - which is what lets the error it guards against be pinned
    # on a bench. That error does not look like an error: every coordinate
    # comes out plausible, self consistent and wrong the same way, and the
    # panel is drilled as a mirror of the one that was drawn.
    ${QET_DIR}/sources/location/drillingorigin.cpp
    ${QET_DIR}/sources/location/drillingorigin.h
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
    # What a rail or a duct is for a shop that cuts its own: a section, a
    # depth and a token, with the length on the piece and not on the bar.
    # Two numbers and a rectangle, no scene and no catalogue - which is what
    # lets "pulling one end does not move the other", "a duct pulled longer
    # does not get wider" and "three pieces of one bar are one line of the
    # list" be settled here rather than by dragging something. What draws the
    # piece and what pushes the undo command stay out, in sources/location/layout
    # and sources/undocommand.
    ${QET_DIR}/sources/location/mountingprofile.cpp
    ${QET_DIR}/sources/location/mountingprofile.h
    # O que está clipsado em qual trilho, e onde a peça para quando encaixa.
    # Retângulo e milímetro, sem cena e sem catálogo -- e está aqui porque o
    # defeito que ela evita só aparece na oficina: um trilho arrastado que
    # deixa os disjuntores para trás produz uma chapa furada onde nada
    # segura doze peças, e nada na tela disse isso. Uma regra de geometria
    # que só se exercita arrastando alguma coisa é uma regra que ninguém
    # confere duas vezes. O gesto -- soltar, arrastar, desfazer -- precisa de
    # cena e fica em src/ui/mountingclipscene_test.cpp, no C_uitests.
    ${QET_DIR}/sources/location/mountingclip.cpp
    ${QET_DIR}/sources/location/mountingclip.h
    # Alinhar e distribuir: média, mínimo e divisão sobre uma lista de
    # retângulos. Está aqui pelo mesmo motivo -- é conta, e conta se confere
    # por número. A que mais erra é a do espaçamento: o vão é a sobra
    # dividida pelo número de ESPAÇOS, que é um a menos que o de peças, e
    # essa diferença de um só aparece quando se soma o resultado. O gesto
    # (qual seleção, qual passo de desfazer) precisa de cena e fica em
    # src/ui/mountingalignscene_test.cpp, no C_uitests.
    ${QET_DIR}/sources/location/mountingalign.cpp
    ${QET_DIR}/sources/location/mountingalign.h
    # When a project database change is announced. As pure as the collection
    # gate rule: a counter and a flag, no Qt at all. What emits the signal is
    # projectDataBase, which needs an open project and therefore lives in
    # C_uitests, in dbnotify_test.cpp.
    ${QET_DIR}/sources/dataBase/updatecoalescer.cpp
    ${QET_DIR}/sources/dataBase/updatecoalescer.h
    # How a cell of a list is written when it holds the separator, a
    # double quote or an end of line. A rule over two strings: no project,
    # no data base, no window -- and it is here because the failure it
    # prevents is silent. A shifted row reads like a well-formed one, so
    # the only place the rule can be pinned is a bench that compares the
    # written text, character for character. The exporters that call it
    # need a project open and stay out.
    ${QET_DIR}/sources/utils/csvwriter.cpp
    ${QET_DIR}/sources/utils/csvwriter.h
    # The rules a bill of materials is made of, as text: which rows of the
    # element tables name something, what a purchase list groups by, and
    # which columns a query built by the export window publishes. Three
    # string rules and no SQLite at all - which is what lets the one that
    # matters be pinned here: exclude_from_bom is in the list of information
    # keys and must never count as information, and a condition that counted
    # it would work on one project and silently stop working on the next.
    # The queries made of them need a project open and stay out.
    ${QET_DIR}/sources/dataBase/bomquery.cpp
    ${QET_DIR}/sources/dataBase/bomquery.h
    # The preferences of the workstation: what an omitted key means, and the
    # three states of the backup copy question. QSettings and QString, no
    # QETApp and no project - which is what lets the defect it was written
    # against be pinned without opening a window. That defect is silent: the
    # preference page read border-columns_0 with an omitted value of its own
    # while the four consumers read it with another, and since the page
    # writes the key on every visit, opening the preferences once renumbered
    # the columns of every folio. Nothing but a bench catches that, because
    # the program behaves impeccably while doing it.
    ${QET_DIR}/sources/utils/apppreferences.cpp
    ${QET_DIR}/sources/utils/apppreferences.h
    # And the dialogue that asks the question, which is here for the same
    # reason the symbol preview is: a widget can be built and clicked without
    # a screen (QT_QPA_PLATFORM=offscreen in main.cpp). It depends on nothing
    # but Qt - no QETApp, no project - and what it does not do is decide: it
    # reports the tick, and the rule above turns the tick and the answer into
    # a stored policy. Who calls it is QETDiagramEditor, which needs a
    # project open and stays out.
    ${QET_DIR}/sources/ui/backupdialog.cpp
    ${QET_DIR}/sources/ui/backupdialog.h
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
    # And what a rule hands back when it finds something. It is here, and
    # not next to the engines, because the case that matters most has no
    # project in it: a rule that speaks about the project as a whole has
    # no component and no folio to point at, and what it must not do is
    # hand back folio 1. Storing the three kinds of target as forward
    # declared pointers is what keeps this file free of Element, Conductor
    # and Diagram - so the pure suite can settle "there is nowhere to go"
    # without linking the program. The walk that fills those pointers with
    # real objects needs a QETProject and stays out, in C_uitests.
    ${QET_DIR}/sources/drc/drcfinding.cpp
    ${QET_DIR}/sources/drc/drcfinding.h
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
    # Which record a value was written on - the part, or the class that
    # declares the property. A value type over Qt Core, no data base of its
    # own: the resolving is Catalog's, next to the flattening it explains.
    ${QET_DIR}/sources/catalog/catalogvalueorigin.cpp
    ${QET_DIR}/sources/catalog/catalogvalueorigin.h
    # Whether a product code is ready to be drawn: a reading over the map
    # of values a part holds, no data base and no window. It is here
    # because it is the one place "nobody measured this" and "this is
    # zero" are told apart, and because the layout, the part dialogue and
    # the report of what is still to be catalogued all have to give the
    # same answer - so the answer is proved once, here.
    ${QET_DIR}/sources/catalog/physicalview.cpp
    ${QET_DIR}/sources/catalog/physicalview.h
    ${QET_DIR}/sources/catalog/catalogrepository.cpp
    ${QET_DIR}/sources/catalog/catalogrepository.h
    ${QET_DIR}/sources/catalog/catalogschema.cpp
    ${QET_DIR}/sources/catalog/catalogschema.h
    ${QET_DIR}/sources/catalog/catalogtable.cpp
    ${QET_DIR}/sources/catalog/catalogtable.h
    ${QET_DIR}/sources/catalog/catalogtablereader.cpp
    ${QET_DIR}/sources/catalog/catalogtablereader.h
    # What a connector has against what the folios draw of it: the reserve.
    # Two lists of strings in, three lists out, so it has the property the
    # catalog core above has and one more - it does not even open the
    # catalogue, it is fed the pinout. That is what lets "nine used, seven
    # reserve", "this label is on no way of this part" and "nobody wrote
    # down how many ways there are" be told apart here rather than on a
    # folio. What reads the pinout out of the part, what collects the pins
    # of one connector and what puts the number in a table all need a
    # project open and stay out.
    ${QET_DIR}/sources/connector/connectorways.cpp
    ${QET_DIR}/sources/connector/connectorways.h
    # Exchanging the way labels of two pins of one connector. Same property
    # again: a list of three strings per pin in, two labels out, so which
    # swap is refused and which label lands where is settled without a folio
    # open. It leans on renumberplan above for the connector key, which is
    # already here for the same reason. What needs a project - the undo
    # stack, and the folio following the number - is ConnectorCheck::swapPins
    # and stays out.
    ${QET_DIR}/sources/connector/connectorswap.cpp
    ${QET_DIR}/sources/connector/connectorswap.h
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
    # E a regra do botão que move uma borne de régua: qual recusa uma
    # leitura da janela merece, e a frase que cada recusa carrega. Mesma
    # propriedade da de cima, e uma a mais -- ela não conhece nem o enum
    # de tipo de borne, só QString e tr(). Está aqui porque o defeito que
    # ela conserta é mudo: um botão que voltava sem mover e sem dizer
    # nada, e o único jeito de garantir que nenhuma saída volte a ser
    # calada é percorrer as recusas por contagem. Quem mostra a frase é a
    # janela da régua, que precisa de projeto aberto e fica de fora.
    ${QET_DIR}/sources/TerminalStrip/terminalmove.cpp
    ${QET_DIR}/sources/TerminalStrip/terminalmove.h
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
    # Which revision a project is in, and whether it is still open (T25).
    # Five fields and a date that is stamped by closing and by nothing
    # else: QString, QDate and QDomElement, no project and no title block,
    # which is what lets the round trip through the project file be pinned
    # before a single command is written against it. The failures it
    # prevents are all silent - an open revision coming back carrying a
    # date, a closed one coming back open, an accent lost in a description
    # - and a silent failure on an emitted drawing is found by the
    # customer. What closes a revision, what locks the project and what
    # draws the revision cloud all need a project open and stay out.
    #
    # PropertiesInterface comes with it: the class implements it, so the
    # constructor and the destructor of the base have to be linked. It is
    # two empty bodies over QSettings and QDomElement, and it arrives in
    # this list for the first time with this file.
    ${QET_DIR}/sources/properties/projectrevision.cpp
    ${QET_DIR}/sources/properties/projectrevision.h
    ${QET_DIR}/sources/properties/propertiesinterface.cpp
    ${QET_DIR}/sources/properties/propertiesinterface.h
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
        # The bookmark tree of an emitted PDF, read back out of the bytes.
        # It needs a project open because what is proved is a whole .qet
        # exported and reopened; and it is here and not beside the pure
        # rules because the tree is written over a file the engine has
        # already closed, so there is nothing to look at until one exists.
        src/ui/pdfoutline_test.cpp
        src/ui/projectopen_test.cpp
        src/ui/appearance_test.cpp
        src/ui/dragassign_test.cpp
        src/ui/iecoff_test.cpp
        src/ui/assignpart_test.cpp
        # Which products of an open project the catalogue cannot draw
        # yet. The rule behind the report, not the report: no suite here
        # opens a QDialog, so the window is left to a person and the
        # list it shows is proved by number.
        src/ui/physicalviewreport_test.cpp
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
        # Starting a project from a template project: what comes across, and
        # above all what must not - the path of the template. It needs a
        # project open because what is proved is a whole .qet read, copied
        # and written somewhere else.
        src/ui/projecttemplate_test.cpp
        src/ui/listengine_test.cpp
        # O que sai da lista de material quando o material tem ponto e
        # virgula, aspas ou quebra de linha no nome. Precisa de projeto
        # aberto porque o que se prova e o arquivo escrito, lido de volta
        # por um leitor proprio -- a regra de escape em si fica um andar
        # abaixo, em csvwriter_test.cpp do C_unittests, e nao se repete
        # aqui.
        src/ui/csvexport_test.cpp
        src/ui/titleblockeditor_test.cpp
        src/ui/navigate_test.cpp
        # Which strip a terminal of the schematic belongs to - the half of
        # the double click a bench can ask. It needs a project open because
        # a strip is made of terminal elements of real folios; the window
        # the click opens needs a QETApp and stays out, which is why the
        # click itself is only sent where it has to do nothing.
        src/ui/terminalnavigate_test.cpp
        # Where a double click on a search result lands. It needs a project
        # open because what is proved is the selection of a real folio after
        # the answer was pointed at; the tree that holds the results is
        # filled from a QETDiagramEditor and stays out, so what is asked is
        # the function the slot calls.
        src/ui/searchresult_test.cpp
        # What a folio reference says is waiting on the other side. It
        # needs a project open because the answer is a walk over the wires
        # of the paired arrow, on another folio; the tooltip it feeds is
        # under a mouse pointer and stays for a person to judge.
        src/ui/reportreferenced_test.cpp
        src/ui/nameslocale_test.cpp
        src/ui/dbnotify_test.cpp
        # Quais das coisas que a folha desenha entram nas tabelas de elemento,
        # e quais delas uma lista mostra -- que não são o mesmo conjunto.
        # Precisa de projeto aberto porque o que se prova é um banco povoado
        # duas vezes, pelo caminho do desenho e pelo da releitura do arquivo,
        # e porque a metade que não pode mudar só é legível num projeto com
        # duzentas setas de folio dentro.
        src/ui/elementtable_test.cpp
        # O que o plano de fiação exportado diz sobre cada fio, célula por
        # célula. Precisa de projeto aberto porque o que se prova é o texto
        # escrito a partir de um .qet de verdade — e porque a metade que
        # importa é a coluna Página, que só existe depois de um projeto ter
        # folhas numeradas. A substituição em si fica um andar abaixo, em
        # foliovariables_test.cpp do C_unittests, e não se repete aqui.
        src/ui/wiringlist_test.cpp
        src/ui/componentlabel_test.cpp
        src/ui/mountinglayoutsave_test.cpp
        # A cena em milímetro da placa de montagem: o que ela conta, o que
        # ela se recusa a fazer item, e o que um arraste deixa na pilha. Está
        # aqui porque uma QGraphicsScene precisa de uma QApplication, e a
        # lista escrita à mão do C_unittests é um contrato de que aquelas
        # regras se calculam sem cena nenhuma. A aritmética da placa fica um
        # andar abaixo, em mountinglayout_test.cpp, e não se repete aqui.
        src/ui/mountingscene_test.cpp
        # O fio entre a cena e o projeto: a janela que lê a face do
        # QETProject ao abrir e a grava de volta a cada passo. Precisa de
        # projeto aberto porque é exatamente isso que se prova — que o
        # arraste chega ao arquivo e volta dele. A cena sozinha fica um
        # arquivo acima, em mountingscene_test.cpp, e não se repete aqui;
        # a janela como o olho a vê não está em suíte nenhuma, e continua
        # sendo roteiro manual.
        src/ui/mountinglayouteditor_test.cpp
        # O trilho e a canaleta no desenho: montar e desmontar sem reconstruir
        # a placa, esticar a peça, e o que cada gesto deixa na pilha de
        # desfazer — mais a ponta que leva o perfil ao projeto e o traz de
        # volta do arquivo. Precisa de cena instanciada e de projeto aberto; a
        # aritmética do perfil fica um andar abaixo, em mountingprofile_test.cpp
        # do C_unittests, e não se repete aqui.
        src/ui/mountedprofileitem_test.cpp
        # O encaixe como gesto: soltar um componente sobre um trilho, arrastar
        # o trilho e levar os doze disjuntores num passo só, e desfazer
        # devolvendo todos. Precisa de cena instanciada porque o que se prova é
        # o que uma lista de retângulos não tem — o sinal do arraste chegando à
        # cena e o passo que ele deixa na pilha. A geometria do encaixe fica um
        # andar abaixo, em mountingclip_test.cpp do C_unittests, e não se
        # repete aqui.
        src/ui/mountingclipscene_test.cpp
        # Alinhar e distribuir como gesto: o que a seleção entrega, o que a
        # cena recusa e com que motivo, e que seis peças alinhadas são um
        # passo de desfazer e não seis. Precisa de cena porque é a seleção
        # dela que se lê; a aritmética fica um andar abaixo, em
        # mountingalign_test.cpp do C_unittests, e não se repete aqui.
        src/ui/mountingalignscene_test.cpp
        # A platine desenhada numa folha do projeto: o que o item lê do
        # QETProject, o que ele redesenha sozinho quando a placa muda, e o
        # que ele nunca escreve de volta. Precisa de projeto aberto porque o
        # fio que se prova e exatamente esse -- um sinal do projeto chegando
        # a um item de folha, e o item sobrevivendo a salvar e reabrir. A
        # aritmetica da placa fica em mountinglayout_test.cpp do
        # C_unittests, e a cena em milimetro em mountingscene_test.cpp;
        # nenhuma das duas se repete aqui.
        src/ui/mountinglayoutview_test.cpp
        # A tabela de furos como janela: que ela lista a face que o projeto
        # guarda, que as colunas da tela sao as colunas do arquivo, que a
        # frase do canto de medicao e a mesma nos dois, e que o filtro
        # estreita o que se olha e nunca o que se entrega. Precisa de projeto
        # aberto porque e a face do QETProject que ela le; a aritmetica da
        # tabela fica um andar abaixo, em drillingtable_test.cpp e
        # drillingorigin_test.cpp do C_unittests, e nao se repete aqui.
        src/ui/drillingtabledialog_test.cpp
        # O painel que mostra o que nao cabe: quantas reclamacoes a regra
        # achou e quantas linhas a janela desenhou, as quatro respostas de
        # MountingFit como quatro frases, e a lista que se refaz quando a
        # peca e arrastada. Precisa de projeto aberto porque e o sinal do
        # projeto que a refaz; a aritmetica fica em mountingcheck_test.cpp do
        # C_unittests, e nao se repete aqui.
        src/ui/mountingcheckdialog_test.cpp
        # O modelo de configuração escrito no .qet e lido de volta. Precisa de
        # projeto aberto porque o que se prova são as duas linhas do
        # QETProject que gravam e leem a árvore — a aritmética dela fica um
        # andar abaixo, em option_test.cpp do C_unittests, e não se repete
        # aqui.
        src/ui/projectoptions_test.cpp
        # Os cinco gestos sobre a árvore de opções como passos da pilha do
        # projeto: quantos passos cada um deixa, como o menu Desfazer os chama,
        # e que a operação que não muda nada não deixa passo nenhum. Precisa de
        # projeto aberto porque a pilha é do projeto — a aritmética da árvore
        # fica um andar abaixo, em option_test.cpp do C_unittests, e não se
        # repete aqui.
        src/ui/optionundo_test.cpp
        src/ui/connectorlink_test.cpp
        src/ui/connectorrenumber_test.cpp
        # Which connectors of an open project nobody can count the ways of,
        # and which pins belong to no connector at all. The rule behind the
        # report and not the report: no suite here opens a QDialog, so the
        # window is left to a person and the two lists it shows are proved by
        # number. The arithmetic of the reserve itself is one floor down, in
        # connectorways_test.cpp of C_unittests, and is not repeated here.
        src/ui/connectorcheck_test.cpp
        # Two pins of one connector trading their numbers on an open project:
        # one step on the stack, the folio following, and nothing moving. The
        # arithmetic of which label lands where is one floor down in
        # connectorswap_test.cpp of C_unittests; what is here is the half a
        # rule over a list cannot have.
        src/ui/connectorswap_test.cpp
        # A régua de bornes continua sendo a mesma régua depois de
        # gravar as propriedades dela. Precisa de projeto aberto porque
        # uma TerminalStrip só existe dentro de um, e é a identidade de
        # que o botão de mover depende: a lista de destinos guarda uuid,
        # e não ponteiro. A frase que a recusa diz é um andar abaixo, em
        # terminalmove_test.cpp do C_unittests.
        src/ui/terminalstripidentity_test.cpp
        # O que a árvore do gerenciador de réguas escreve em cada linha.
        # Precisa de projeto aberto porque o que se prova é o texto que o
        # QTreeWidgetItem recebe de um QETProject carregado, e porque o
        # recuo do borne sem rótulo é a posição dele na folha -- sem folha
        # não há o que mostrar. A linha em branco não era defeito de
        # leitura: o dado está vazio no arquivo, e o caso mede as duas
        # coisas de uma vez.
        src/ui/terminalstriptree_test.cpp
        # O que as colunas de cabo, de via e de condutor da régua escrevem em
        # cada linha. Precisa de projeto aberto porque as três respostas saem
        # de um condutor desenhado numa folha e de um cabo que pertence ao
        # projeto -- sem folha não há condutor, e sem projeto não há cabo a
        # que ele pertença. As duas primeiras colunas saíam vazias para todo
        # borne de todo projeto, e vazio não se distingue de "este borne não
        # está em cabo nenhum": é por isso que o caso mede o conteúdo de cada
        # célula e guarda os dois vazios legítimos.
        src/ui/terminalstripcable_test.cpp
        # O motor da rota SQL da verificacao de projeto: o que ele
        # pergunta ao banco do projeto, o que ele se recusa a perguntar,
        # e como ele distingue uma consulta que quebrou de uma que nao
        # achou nada. Precisa de projeto aberto porque o que se prova e
        # uma pergunta feita a um banco com linhas dentro; o contrato de
        # regra, que e aritmetica sobre uma lista, fica um andar abaixo
        # em drcrule_test.cpp do C_unittests e nao se repete aqui.
        src/ui/drcsql_test.cpp
        # As tres primeiras regras escritas contra o contrato -- bobina sem
        # contato, contato sem bobina, borne fora de regua -- mais o motor
        # da rota de varredura que as roda e o painel que leva da linha ao
        # componente. Precisa de projeto aberto por medicao, e nao por
        # gosto: o contato escravo nao tem linha em tabela nenhuma do banco
        # do projeto, e o borne so chega a tabela dele depois que alguem
        # desenha um condutor -- qualquer uma das tres escrita em SQL
        # devolveria lista vazia e declararia o projeto limpo. Cada regra
        # traz as duas metades, a que acusa e a que se cala de proposito,
        # sobre a mesma bancada antes e depois do vinculo.
        src/ui/drclinkrules_test.cpp
        # Quantas etiquetas a lista de numeros de fio manda imprimir, e
        # para qual numero. Precisa de projeto aberto porque a regra le
        # duas coisas que so o desenho decide -- o texto do condutor e o
        # tipo de elemento em que cada ponta esta encaixada --, e
        # condutores montados em memoria deixariam o caso escolher as
        # duas, que e afirmar a regra contra ela mesma.
        src/ui/wirenumberlist_test.cpp
        # Por quantos itens uma linha da lista de compra pode responder.
        # Precisa de projeto aberto porque o que se prova e o agrupamento
        # de uma consulta sobre o banco do projeto, com codigo de peca,
        # revisao e designacao dentro; a citacao de celula, que e regra
        # sobre duas strings, fica em csvwriter_test.cpp do C_unittests e
        # a saida citada em csvexport_test.cpp, e nenhuma se repete aqui.
        src/ui/bomgrouping_test.cpp
        # Qual linha da lista de material nomeia alguma coisa, e qual delas e
        # marcador desenhado. Precisa de projeto aberto porque o que se prova
        # e a visao do banco aplicada a uma folha de verdade -- o marcador de
        # fim de regua e do mesmo tipo Terminal que o borne ao lado dele, e
        # separar os dois so e demonstravel sobre elementos carregados. A
        # condicao em si, que e regra sobre strings, fica em bomquery_test.cpp
        # do C_unittests e nao se repete aqui.
        src/ui/bomnameless_test.cpp
        # O apostrofo no nome de uma localizacao e a consulta que o bloco
        # da folha guarda dentro do projeto. Precisa de projeto aberto
        # porque a prova nao e o texto da consulta e sim a execucao dela
        # contra o banco de onde ela foi escrita: antes do conserto ela
        # nao roda, e ninguem era avisado.
        src/ui/locationbomquery_test.cpp)

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
