cmake_minimum_required( VERSION 3.6 )

project( HDK_Project )

# CMAKE_PREFIX_PATH must contain the path to the toolkit/cmake subdirectory of
# the Houdini installation. See the "Compiling with CMake" section of the HDK
# documentation for more details, which describes several options for
# specifying this path.
list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )


set(CURL_LIB_PATH "$ENV{HFS}/custom/houdini/dsolib/libcurl.lib")


# Locate Houdini's libraries and header files.
# Registers an imported library target named 'Houdini'.
find_package( Houdini REQUIRED )

set( library_name CMD_PolyHaven)


# Add a library and its source files.
add_library( ${library_name} SHARED
    CMD_PolyHaven.cpp
    abspath.h
    abspath.cpp
    constants.h
    download_file.cpp
    download_file.h
    filehash.cpp
    filehash.h
    get_asset_lib.cpp
    get_asset_lib.h
    get_asset_list.cpp
    get_asset_list.h
    ui/AssetDelegate.cpp
    ui/AssetDelegate.h
    ui/AssetDownloadTask.cpp
    ui/AssetDownloadTask.h
    ui/AssetModel.cpp
    ui/AssetModel.h
    ui/phaPullFromPolyhaven.cpp
    ui/phaPullFromPolyhaven.h
    ui/startwindow.cpp
    ui/startwindow.h
)

target_compile_definitions(Houdini INTERFACE QT_NO_KEYWORDS)

# Link against the Houdini libraries, and add required include directories and
# compile definitions.
target_link_libraries( ${library_name} Houdini HoudiniThirdParty ${CURL_LIB_PATH} )

# Include ${CMAKE_CURRENT_BINARY_DIR} for the generated header.
target_include_directories( ${library_name} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    "E:/curl-8.9.1/include"
)

# Sets several common target properties, such as the library's output directory.
houdini_configure_target( ${library_name} )
