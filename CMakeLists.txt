cmake_minimum_required(VERSION 3.13)

project(maya2glTF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Maya Devkit (AUTOMATIC DETECTION)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules")
find_package(Maya REQUIRED)

include(ExternalProject)

# ---------------------------------------------------------------------------
# COLLADA2GLTF External Project
# ---------------------------------------------------------------------------
ExternalProject_Add(COLLADA2GLTF
  GIT_REPOSITORY https://github.com/iimachines/COLLADA2GLTF
  GIT_TAG Maya2glTF
  PREFIX COLLADA2GLTF
  SOURCE_SUBDIR GLTF
  INSTALL_DIR
  CMAKE_ARGS
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  CMAKE_CACHE_ARGS
    "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true"
)

# cpp-linq
ExternalProject_Add(linq
  GIT_REPOSITORY https://github.com/coveo/linq.git
  GIT_TAG v2.0.2
  PREFIX linq
  INSTALL_DIR
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

# GSL
ExternalProject_Add(GSL
  GIT_REPOSITORY https://github.com/Microsoft/GSL.git
  GIT_TAG v2.0.0
  PREFIX GSL
  INSTALL_DIR
  CMAKE_ARGS
    -DGSL_TEST=OFF
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)

# filesystem
ExternalProject_Add(filesystem
  GIT_REPOSITORY https://github.com/gulrak/filesystem
  GIT_TAG v1.2.6
  PREFIX filesystem
  INSTALL_DIR
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

# ---------------------------------------------------------------------------
# INCLUDE PATHS
# ---------------------------------------------------------------------------
set(GLTF_INCLUDE_DIR          "${CMAKE_BINARY_DIR}/COLLADA2GLTF/src/COLLADA2GLTF/GLTF/include")
set(DRACO_INCLUDE_DIR         "${CMAKE_BINARY_DIR}/COLLADA2GLTF/src/COLLADA2GLTF/GLTF/dependencies/draco/src")
set(RAPIDJSON_INCLUDE_DIR     "${CMAKE_BINARY_DIR}/COLLADA2GLTF/src/COLLADA2GLTF/GLTF/dependencies/rapidjson/include")
set(GSL_INCLUDE_DIR           "${CMAKE_BINARY_DIR}/GSL/include")
set(LINQ_INCLUDE_DIR          "${CMAKE_BINARY_DIR}/linq/src/linq/lib")
set(FS_INCLUDE_DIR            "${CMAKE_BINARY_DIR}/filesystem/src/filesystem/include")

ExternalProject_Get_Property(COLLADA2GLTF binary_dir)
set(GLTF_LIBRARY_DIR "${binary_dir}/${CMAKE_BUILD_TYPE}") 
set(DRACO_LIBRARY_DIR "${binary_dir}/dependencies/draco/${CMAKE_BUILD_TYPE}") 

# ---------------------------------------------------------------------------
# Versioning
# ---------------------------------------------------------------------------
execute_process(
  COMMAND git rev-parse --short HEAD
  OUTPUT_VARIABLE GIT_SHORT_HASH
)
string(REGEX REPLACE "\n$" "" GIT_SHORT_HASH "${GIT_SHORT_HASH}")

execute_process(
  COMMAND git tag -l --points-at HEAD
  OUTPUT_VARIABLE GIT_TAG
)
string(REGEX REPLACE "\n$" "" GIT_TAG "${GIT_TAG}")

if(NOT GIT_TAG)
  set(GIT_TAG "main")
endif()

set(ARCHIVE_VERSION "${GIT_TAG}_${GIT_SHORT_HASH}")

message("GIT version = ${ARCHIVE_VERSION}")

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/maya/maya2glTF_version.mel.in
  ${CMAKE_CURRENT_BINARY_DIR}/maya2glTF_version.mel
)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

file(GLOB SOURCES "src/*.cpp" "src/*.h" "src/*.c")

# ---------------------------------------------------------------------------
# MSVC Precompiled Headers
# ---------------------------------------------------------------------------
if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Yuexternals.h")
  set_source_files_properties(src/externals.cpp PROPERTIES COMPILE_FLAGS "/Ycexternals.h")
  set_source_files_properties(src/mikktspace.c  PROPERTIES COMPILE_FLAGS "/Y-")
  set_source_files_properties(src/PolarDecomposition.c  PROPERTIES COMPILE_FLAGS "/Y-")
endif()

# ---------------------------------------------------------------------------
# Include + Link Directories
# ---------------------------------------------------------------------------
include_directories(
  ${GLTF_INCLUDE_DIR}
  ${DRACO_INCLUDE_DIR}
  ${RAPIDJSON_INCLUDE_DIR}
  ${GSL_INCLUDE_DIR}
  ${MAYA_INCLUDE_DIR}
  ${LINQ_INCLUDE_DIR}
  ${FS_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

link_directories(
  ${MAYA_LIBRARY_DIR}
  ${GLTF_LIBRARY_DIR}
  ${DRACO_LIBRARY_DIR}
)

# ---------------------------------------------------------------------------
# Build Plugin
# ---------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED ${SOURCES})

add_dependencies(${PROJECT_NAME}
  GSL
  COLLADA2GLTF
  linq
  filesystem
)

target_link_libraries(${PROJECT_NAME} ${MAYA_LIBRARIES} GLTF draco)

# ---------------------------------------------------------------------------
# Deployment
# ---------------------------------------------------------------------------
if(MSVC)
  GET_FILENAME_COMPONENT(USER_DOCUMENTS "[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders;Personal]" ABSOLUTE CACHE)
  message("-- Found user documents folder = ${USER_DOCUMENTS}")

  set(MAYA_OUTPUT_DIR "$<SHELL_PATH:${USER_DOCUMENTS}/maya")
  add_custom_command(TARGET maya2glTF POST_BUILD 
    COMMAND xcopy /y "$<SHELL_PATH:$<TARGET_FILE:maya2glTF>>" "${MAYA_OUTPUT_DIR}/${MAYA_VERSION}/plug-ins/>"
    COMMAND xcopy /y "$<SHELL_PATH:${CMAKE_CURRENT_BINARY_DIR}/maya2glTF_version.mel>" "${MAYA_OUTPUT_DIR}/scripts/>"
    COMMAND xcopy /y "$<SHELL_PATH:${PROJECT_SOURCE_DIR}/maya/scripts/maya2glTF*.*>" "${MAYA_OUTPUT_DIR}/scripts/>"
    COMMAND xcopy /y /s /i "$<SHELL_PATH:${PROJECT_SOURCE_DIR}/maya/renderData/*.*>" "${MAYA_OUTPUT_DIR}/maya2glTF/PBR/>"
    COMMAND xcopy /y /s /i "$<SHELL_PATH:${PROJECT_SOURCE_DIR}/maya/prefs/*.*>" "${MAYA_OUTPUT_DIR}/${MAYA_VERSION}/prefs/>"
    COMMAND if exist "$<SHELL_PATH:$<TARGET_PDB_FILE:maya2glTF>>" (xcopy /y /c "$<SHELL_PATH:$<TARGET_PDB_FILE:maya2glTF>>" "${MAYA_OUTPUT_DIR}/${MAYA_VERSION}/plug-ins/>")
  )
endif()

# ---------------------------------------------------------------------------
# Redist Packaging (Installer .bat + plugin files)
# ---------------------------------------------------------------------------
set(REDIST_DIR "${CMAKE_BINARY_DIR}/redist")
file(MAKE_DIRECTORY "${REDIST_DIR}")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${REDIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/maya2glTF_version.mel" "${REDIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/maya/scripts" "${REDIST_DIR}/scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/maya/renderData" "${REDIST_DIR}/renderData"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/maya/prefs" "${REDIST_DIR}/prefs"
    COMMENT "Staging redist package in ${REDIST_DIR}"
)

# The PDB only exists in Debug/RelWithDebInfo builds, so guard the copy
# (an unconditional copy aborts the whole post-build chain in Release).
if(MSVC)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND if exist "$<SHELL_PATH:$<TARGET_PDB_FILE:${PROJECT_NAME}>>" (xcopy /y /c "$<SHELL_PATH:$<TARGET_PDB_FILE:${PROJECT_NAME}>>" "$<SHELL_PATH:${REDIST_DIR}>\\")
  )
endif()

# Generate installer .bat (copies from its own folder, so redist is relocatable)
set(PLUGIN_FILE_NAME "${PROJECT_NAME}${MAYA_PLUGIN_EXTENSION}")
configure_file(
    "${PROJECT_SOURCE_DIR}/installer.bat.in"
    "${REDIST_DIR}/Maya2glTF_Installer.bat"
    @ONLY
    NEWLINE_STYLE CRLF
)

MAYA_PLUGIN(${PROJECT_NAME})
