find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

qt_add_executable(kvdb-gui
    main.cpp
    main_window.cpp main_window.hpp
    console_widget.cpp console_widget.hpp
    kvdb_client.cpp kvdb_client.hpp
)

target_link_libraries(kvdb-gui PRIVATE Qt6::Widgets Qt6::Network)
target_compile_options(kvdb-gui PRIVATE -Wall -Wextra)
set_target_properties(kvdb-gui PROPERTIES MACOSX_BUNDLE ON)
