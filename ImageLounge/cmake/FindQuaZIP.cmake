# QUAZIP_FOUND - system has the QUAZIP library
# QUAZIP_INCLUDE_DIRECTORY - the QUAZIP include directory
# QUAZIP_LIBRARIES - The libraries needed to use QUAZIP


if(QUAZIP_INCLUDE_DIRECTORY AND QUAZIP_LIBRARIES)
  set(QUAZIP_FOUND TRUE)
else()
  find_path(QUAZIP_INCLUDE_DIRECTORY NAMES quazip/quazip.h)

  find_library(QUAZIP_LIBRARIES NAMES quazip)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(QUAZIP DEFAULT_MSG QUAZIP_INCLUDE_DIRECTORY QUAZIP_LIBRARIES)

  mark_as_advanced(QUAZIP_INCLUDE_DIRECTORY QUAZIP_LIBRARIES)
endif(QUAZIP_INCLUDE_DIRECTORY AND QUAZIP_LIBRARIES)


