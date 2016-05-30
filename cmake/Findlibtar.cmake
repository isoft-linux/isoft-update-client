find_path(LIBTAR_INCLUDE_DIR NAMES libtar.h)
find_library(LIBTAR_LIBRARIES NAMES tar)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libtar REQUIRED_VARS 
                                  LIBTAR_INCLUDE_DIR 
                                  LIBTAR_LIBRARIES)
