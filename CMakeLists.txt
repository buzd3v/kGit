cmake_minimum_required(VERSION 3.22)
project(kGit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# Collect all source files
file(GLOB_RECURSE UI_SOURCES   "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")
file(GLOB_RECURSE UTIL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp")

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/git_engine.cpp
    ${UI_SOURCES}
    ${UTIL_SOURCES}
)

add_executable(kgit ${SOURCES})

target_include_directories(kgit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GTKMM_INCLUDE_DIRS}
    ${LIBGIT2_INCLUDE_DIRS}
)
target_link_directories(kgit PRIVATE
    ${GTKMM_LIBRARY_DIRS}
    ${LIBGIT2_LIBRARY_DIRS}
)
target_link_libraries(kgit PRIVATE
    ${GTKMM_LIBRARIES}
    ${LIBGIT2_LIBRARIES}
    pthread
)
target_compile_options(kgit PRIVATE ${GTKMM_CFLAGS_OTHER} ${LIBGIT2_CFLAGS_OTHER})

# Install binary
install(TARGETS kgit DESTINATION bin)

# Install Nautilus desktop action files
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/actions/kgit-clone.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/data/actions/kgit-open.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/data/actions/kgit-init.desktop
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/file-manager/actions/")

# Install icon
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/icons/kgit.svg
    DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps/")

# Post-install: refresh desktop database (optional, harmless if missing)
install(CODE "execute_process(COMMAND update-desktop-database
    ${CMAKE_INSTALL_PREFIX}/share/applications
    ERROR_QUIET OUTPUT_QUIET)")
