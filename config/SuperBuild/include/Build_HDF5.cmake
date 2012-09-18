#  -*- mode: cmake -*-

#
# Build TPL:  HDF5 
#    
# --- Define all the directories and common external project flags
define_external_project_args(HDF5
                             TARGET hdf5
                             DEPENDS ${MPI_PROJECT} ZLIB)

# --- Define configure parameters

# Use the common cflags, cxxflags
include(BuildWhitespaceString)
build_whitespace_string(hdf5_cflags
                       -I${TPL_INSTALL_PREFIX}/include
                       ${Amanzi_COMMON_CFLAGS})

build_whitespace_string(hdf5_cxxflags
                       -I${TPL_INSTALL_PREFIX}/include
                       ${Amanzi_COMMON_CXXFLAGS})
set(cpp_flag_list 
    -I${TPL_INSTALL_PREFIX}/include
    ${Amanzi_COMMON_CFLAGS}
    ${Amanzi_COMMON_CXXFLAGS})
list(REMOVE_DUPLICATES cpp_flag_list)
build_whitespace_string(hdf5_cppflags ${cpp_flags_list})

# Add
if ( ( NOT BUILD_MPI) AND ( NOT MPI_WRAPPERS_IN_USE ) AND (MPI_C_LIBRARIES) )
  build_whitespace_string(hdf5_ldflags -L${TPL_INSTALL_PREFIX}/lib ${MPI_C_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})
else()
  build_whitespace_string(hdf5_ldflags -L${TPL_INSTALL_PREFIX}/lib ${CMAKE_EXE_LINKER_FLAGS})
endif()  

# --- Add external project build and tie to the ZLIB build target
ExternalProject_Add(${HDF5_BUILD_TARGET}
                    DEPENDS   ${HDF5_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${HDF5_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${HDF5_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}              # Download directory
                    URL          ${HDF5_URL}                      # URL may be a web site OR a local file
                    URL_MD5      ${HDF5_MD5_SUM}                  # md5sum of the archive file
                    # -- Configure
                    SOURCE_DIR       ${HDF5_source_dir}           # Source directory
                    CONFIGURE_COMMAND
                                     <SOURCE_DIR>/configure
                                                 --prefix=<INSTALL_DIR>
                                                 --disable-fortran
                                                 --disable-cxx
                                                 --enable-production
                                                 --enable-largefile
                                                 --enable-parallel
                                                 --with-zlib=${TPL_INSTALL_PREFIX}
                                                 CC=${CMAKE_C_COMPILER_USE}
                                                 CFLAGS=${hdf5_cflags}
                                                 CXX=${CMAKE_CXX_COMPILER_USE}
                                                 CXXFLAGS=${hdf5_cxxflags}
                                                 CPPFLAGS=${hdf5_cppflags}
                                                 LDFLAGS=${hdf5_ldflags}
                    # -- Build
                    BINARY_DIR        ${HDF5_build_dir}           # Build directory 
                    BUILD_COMMAND     $(MAKE)                     # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE   ${HDF5_BUILD_IN_SOURCE}     # Flag for in source builds
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}        # Install directory
                    # -- Output control
                    ${HDF5_logging_args})

# --- Useful variables for packages that depend on HDF5 (NetCDF)
include(BuildLibraryName)
build_library_name(hdf5 HDF5_C_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
build_library_name(hdf5_hl HDF5_HL_LIBRARY APPEND_PATH ${TPL_INSTALL_PREFIX}/lib)
set(HDF5_LIBRARIES ${HDF5_HL_LIBRARY} ${HDF5_C_LIBRARY} ${ZLIB_LIBRARIES} m)
set(HDF5_INCLUDE_DIRS ${TPL_INSTALL_PREFIX}/include ${ZLIB_INCLUDE_DIRS})
list(REMOVE_DUPLICATES HDF5_INCLUDE_DIRS)
