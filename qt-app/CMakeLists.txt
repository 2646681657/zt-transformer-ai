cmake_minimum_required(VERSION 3.21)
project(ztf-designer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RC_COMPILER "D:/QT/Tools/mingw1310_64/bin/windres.exe")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Concurrent PrintSupport Network)
qt_standard_project_setup()

file(GLOB_RECURSE CORE_SOURCES src/core/*.cpp src/core/*.h)
file(GLOB_RECURSE ENGINE_SOURCES src/engine/*.cpp src/engine/*.h)
file(GLOB_RECURSE ADAPTER_SOURCES src/adapter/*.cpp src/adapter/*.h)
file(GLOB_RECURSE GUI_SOURCES src/gui/*.cpp src/gui/*.h)

qt_add_executable(ztf-designer WIN32
    src/main.cpp
    ${CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${ADAPTER_SOURCES}
    ${GUI_SOURCES}
    resources/resources.qrc
    app.rc
)

target_include_directories(ztf-designer PRIVATE
    src
    src/core
    src/engine
    src/adapter
    src/gui
    src/gui/pages
    src/gui/widgets
    src/gui/dialogs
)

target_link_libraries(ztf-designer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Concurrent
    Qt6::PrintSupport
    Qt6::Network
)

set_target_properties(ztf-designer PROPERTIES
    OUTPUT_NAME "ZTBLD-Designer"
    WIN32_EXECUTABLE TRUE
)

# 临时扫描工具（SB22-M/630 可行域搜索，验证后删除）
add_executable(scan-sb22
    tests/scan_sb22.cpp
    src/engine/ElectromagneticEngine.cpp
    src/adapter/DesignDatabase.cpp
    resources/resources.qrc
)
set_target_properties(scan-sb22 PROPERTIES AUTOMOC OFF)
target_include_directories(scan-sb22 PRIVATE
    src
    src/core
    src/engine
    src/adapter
)
target_link_libraries(scan-sb22 PRIVATE
    Qt6::Core
)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(test-startup-schemes tests/startup_scheme_library_test.cpp resources/resources.qrc)
    set_target_properties(test-startup-schemes PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-startup-schemes PRIVATE src/core)
    target_link_libraries(test-startup-schemes PRIVATE Qt6::Core)
    add_test(NAME startup-scheme-library COMMAND test-startup-schemes)
    qt_add_executable(test-candidate-comparison tests/candidate_comparison_test.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-candidate-comparison PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-candidate-comparison PRIVATE src src/core src/engine src/adapter)
    target_link_libraries(test-candidate-comparison PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME candidate-comparison-records COMMAND test-candidate-comparison)
    set_tests_properties(candidate-comparison-records PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    qt_add_executable(test-candidate-comparison-ui tests/candidate_comparison_ui_test.cpp
        src/gui/widgets/CandidateComparisonDialog.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-candidate-comparison-ui PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-candidate-comparison-ui PRIVATE src src/core src/engine src/adapter src/gui/widgets)
    target_link_libraries(test-candidate-comparison-ui PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME candidate-comparison-ui COMMAND test-candidate-comparison-ui)
    set_tests_properties(candidate-comparison-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    add_test(NAME candidate-comparison-integration COMMAND test-candidate-comparison-ui --integration)
    set_tests_properties(candidate-comparison-integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 SKIP_RETURN_CODE 77)
    qt_add_executable(test-data-preparation tests/data_preparation_test.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-data-preparation PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-data-preparation PRIVATE src src/core src/engine src/adapter)
    target_link_libraries(test-data-preparation PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME learning-data-preparation COMMAND test-data-preparation)
    set_tests_properties(learning-data-preparation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    qt_add_executable(test-preparation-ui tests/learning_preparation_ui_test.cpp
        src/gui/widgets/LearningPreparationPanel.cpp src/gui/widgets/LearningWorkspace.cpp
        src/gui/widgets/LearningTrainingPanel.cpp src/gui/widgets/LearningReviewPanel.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-preparation-ui PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-preparation-ui PRIVATE src src/core src/engine src/adapter src/gui/widgets)
    target_link_libraries(test-preparation-ui PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME learning-data-preparation-ui COMMAND test-preparation-ui)
    set_tests_properties(learning-data-preparation-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    qt_add_executable(test-model-review tests/model_review_test.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-model-review PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-model-review PRIVATE src src/core src/engine src/adapter)
    target_link_libraries(test-model-review PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME model-review-registry COMMAND test-model-review)
    set_tests_properties(model-review-registry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    qt_add_executable(test-learning-training tests/learning_training_test.cpp
        src/gui/widgets/LearningTrainingPanel.cpp src/gui/widgets/LearningReviewPanel.cpp ${CORE_SOURCES} ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp resources/resources.qrc)
    set_target_properties(test-learning-training PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-learning-training PRIVATE src src/core src/engine src/adapter src/gui/widgets)
    target_link_libraries(test-learning-training PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME learning-training-process COMMAND test-learning-training)
    set_tests_properties(learning-training-process PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 40)
    add_test(NAME learning-training-integration COMMAND test-learning-training --integration)
    set_tests_properties(learning-training-integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120 SKIP_RETURN_CODE 77)
    qt_add_executable(test-self-learning
        tests/self_learning_test.cpp
        src/gui/widgets/LearningWorkspace.cpp
        src/gui/widgets/LearningTrainingPanel.cpp
        src/gui/widgets/LearningReviewPanel.cpp
        src/gui/widgets/LearningPreparationPanel.cpp
        ${CORE_SOURCES}
        ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp
        resources/resources.qrc
    )
    set_target_properties(test-self-learning PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-self-learning PRIVATE src src/core src/engine src/adapter src/gui/widgets)
    target_link_libraries(test-self-learning PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
    add_test(NAME self-learning-foundation COMMAND test-self-learning)
    set_tests_properties(self-learning-foundation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(test-core-selection
        tests/core_selection_test.cpp
        ${CORE_SOURCES}
        ${ENGINE_SOURCES}
        src/adapter/DesignDatabase.cpp
        src/gui/widgets/ParamTableWidget.cpp
        src/gui/widgets/SchemeCalculationSheet.cpp
        src/gui/widgets/EmResultPanel.cpp
        resources/resources.qrc
    )
    set_target_properties(test-core-selection PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(test-core-selection PRIVATE
        src src/core src/engine src/adapter src/gui/widgets
    )
    target_link_libraries(test-core-selection PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network
    )
    add_test(NAME core-selection-state COMMAND test-core-selection)
    set_tests_properties(core-selection-state PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(test-enter-calc-page
        tests/enter_calc_page_test.cpp
        ${CORE_SOURCES}
        ${ENGINE_SOURCES}
        ${ADAPTER_SOURCES}
        ${GUI_SOURCES}
        resources/resources.qrc
    )
    set_target_properties(test-enter-calc-page PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_compile_definitions(test-enter-calc-page PRIVATE ZTBLD_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(test-enter-calc-page PRIVATE
        src src/core src/engine src/adapter src/gui src/gui/pages src/gui/widgets src/gui/dialogs
    )
    target_link_libraries(test-enter-calc-page PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network
    )
    add_test(NAME enter-calc-page-state COMMAND test-enter-calc-page)
    add_test(NAME dashboard-direct-calculation COMMAND test-enter-calc-page --direct-calculation)
    set_tests_properties(dashboard-direct-calculation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    add_test(NAME startup-scheme-ui COMMAND test-enter-calc-page --startup-schemes)
    set_tests_properties(startup-scheme-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 35)
    add_test(NAME linked-excel-export COMMAND test-enter-calc-page --worksheet-output)
    set_tests_properties(linked-excel-export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(WIN32 AND Python3_Interpreter_FOUND)
        add_test(NAME linked-excel-process-inputs
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_linked_export_formulas.py")
        set_tests_properties(linked-excel-process-inputs PROPERTIES TIMEOUT 120)
    endif()
    set_tests_properties(enter-calc-page-state PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 25)
endif()

if(BUILD_TESTING)
 qt_add_executable(test-unified-default-engine tests/unified_default_engine_test.cpp ${CORE_SOURCES} ${ENGINE_SOURCES} src/adapter/DesignDatabase.cpp resources/resources.qrc)
 set_target_properties(test-unified-default-engine PROPERTIES EXCLUDE_FROM_ALL TRUE)
 target_include_directories(test-unified-default-engine PRIVATE src src/core src/engine src/adapter src/gui/widgets)
 target_link_libraries(test-unified-default-engine PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Concurrent Qt6::PrintSupport Qt6::Network)
 add_test(NAME unified-default-engine COMMAND test-unified-default-engine)
endif()
