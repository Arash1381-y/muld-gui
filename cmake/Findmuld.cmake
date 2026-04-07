# cmake/Findmuld.cmake
set(MULD_ROOT "${CMAKE_SOURCE_DIR}/external/muld")

find_path(MULD_INCLUDE_DIR
    NAMES muld/muld.h
    PATHS ${MULD_ROOT}/include
    NO_DEFAULT_PATH
)

find_library(MULD_LIBRARY
    NAMES muld
    PATHS ${MULD_ROOT}/lib
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(muld
    REQUIRED_VARS MULD_LIBRARY MULD_INCLUDE_DIR
)

if(muld_FOUND AND NOT TARGET muld::muld)
    add_library(muld::muld STATIC IMPORTED)
    set_target_properties(muld::muld PROPERTIES
        IMPORTED_LOCATION "${MULD_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MULD_INCLUDE_DIR}"
    )
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    set_property(TARGET muld::muld APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    )
endif()
