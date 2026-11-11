cmake_minimum_required(VERSION 3.12)

project(QHexView
    LANGUAGES CXX
)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_library(qhexview-lib STATIC
    document/buffer/qfilebuffer.cpp
    document/buffer/qhexbuffer.cpp
    document/buffer/qmemorybuffer.cpp
    document/buffer/qmemoryrefbuffer.cpp
    document/commands/hexcommand.cpp
    document/commands/insertcommand.cpp
    document/commands/removecommand.cpp
    document/commands/replacecommand.cpp
    document/qhexcursor.cpp
    document/qhexdocument.cpp
    document/qhexmetadata.cpp
    document/qhexrenderer.cpp
    qhexview.cpp
)

set_target_properties(qhexview-lib PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(qhexview-lib PRIVATE Qt6::Widgets)
target_include_directories(qhexview-lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(qhexview-tests tests/RenderingTest.cpp)
    set_target_properties(qhexview-tests PROPERTIES AUTOMOC ON CXX_STANDARD 11)
    target_link_libraries(qhexview-tests PRIVATE qhexview-lib Qt6::Widgets Qt6::Test)
    add_test(NAME qhexview-rendering COMMAND qhexview-tests)
    set_tests_properties(qhexview-rendering PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()
