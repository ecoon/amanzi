#  -*- mode: cmake -*-

#
# Build TPL: OpenMPI 
# 

# --- Define all the directories and common external project flags
define_external_project_args(OpenMPI TARGET openmpi)

# Build compiler *FLAGS strings. Pick up the CMAKE_BUILD_TYPE flags
include(BuildWhitespaceString)
build_whitespace_string(openmpi_cflags 
                        -I${TPL_INSTALL_PREFIX}/include ${Amanzi_COMMON_CFLAGS} )
build_whitespace_string(openmpi_cxxflags 
                        -I${TPL_INSTALL_PREFIX}/include ${Amanzi_COMMON_CXXFLAGS} )
build_whitespace_string(openmpi_fcflags 
                        -I${TPL_INSTALL_PREFIX}/include ${Amanzi_COMMON_FCFLAGS} )

# --- Add RPATH to the link flags for the compiler wrappers
set(openmpi_extra_ldflags "-Wl,-rpath,${TPL_INSTALL_PREFIX}/lib")
print_variable(openmpi_extra_ldflags)
find_package(Threads)


				   #                 CFLAGS=${openmpi_cflags}
				   # CXXFLAGS=${openmpi_cxxflags}
				   #FCFLAGS=${openmpi_fcflags}
# --- Add external project build and tie to the OpenMPI build target
ExternalProject_Add(${OpenMPI_BUILD_TARGET}
                    DEPENDS   ${OpenMPI_PACKAGE_DEPENDS}             # Package dependency target
                    TMP_DIR   ${OpenMPI_tmp_dir}                     # Temporary files directory
                    STAMP_DIR ${OpenMPI_stamp_dir}                   # Timestamp and log directory
                    # -- Download and URL definitions
                    DOWNLOAD_DIR ${TPL_DOWNLOAD_DIR}                 # Download directory
                    URL          ${OpenMPI_URL}                      # URL may be a web site OR a local file
                    URL_MD5      ${OpenMPI_MD5_SUM}                  # md5sum of the archive file
                    # -- Configure
                    SOURCE_DIR       ${OpenMPI_source_dir}           # Source directory
                    CONFIGURE_COMMAND
                                   <SOURCE_DIR>/configure
                                                --prefix=<INSTALL_DIR>
						--enable-option-checking
                                                --enable-mpi-f77
                                                --enable-mpi-f90
                                                --enable-mpi-cxx
                                                --enable-binaries
						--enable-shared
						--enable-static
						--with-wrapper-ldflags=${openmpi_extra_ldflags}
                                                CC=${CMAKE_C_COMPILER}
                                                CXX=${CMAKE_CXX_COMPILER}
                                                FC=${CMAKE_Fortran_COMPILER}
                    # -- Build
                    BINARY_DIR        ${OpenMPI_build_dir}           # Build directory 
                    BUILD_COMMAND     $(MAKE)                     # $(MAKE) enables parallel builds through make
                    BUILD_IN_SOURCE   ${OpenMPI_BUILD_IN_SOURCE}     # Flag for in source builds
                    # -- Install
                    INSTALL_DIR      ${TPL_INSTALL_PREFIX}        # Install directory
                    # -- Output control
                    ${OpenMPI_logging_args})

# --- Define variables pointing to compiler wrappers and parallel run commond
set(MPI_C_COMPILER        ${TPL_INSTALL_PREFIX}/bin/mpicc)
set(MPI_CXX_COMPILER      ${TPL_INSTALL_PREFIX}/bin/mpicxx)
set(MPI_Fortran_COMPILER  ${TPL_INSTALL_PREFIX}/bin/mpif90)
set(MPIEXEC               ${TPL_INSTALL_PREFIX}/bin/mpirun)
set(MPI_EXEC              ${TPL_INSTALL_PREFIX}/bin/mpirun)
