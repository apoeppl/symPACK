set(UPCXX_PREFIX $ENV{UPCXX_DIR})
if (UPCXX_PREFIX)
  message("UPCXX has been provided, not compiling local version.")

set(UPCXX_INCLUDE_DIR ${UPCXX_PREFIX}/include ${GASNET_INCLUDE_DIR} ${GASNET_CONDUIT_INCLUDE_DIR})
set(UPCXX_LIBRARY_PATH ${UPCXX_PREFIX}/lib)

add_library(libupcxx STATIC IMPORTED)

set_property(TARGET libupcxx PROPERTY IMPORTED_LOCATION ${UPCXX_PREFIX}/lib/libupcxx.a)

set(UPCXX_LIBRARIES ${GASNET_LIBRARIES})
set(UPCXX_LIBRARY_PATH ${UPCXX_LIBRARY_PATH} ${GASNET_LIBRARY_PATH})
  set(UPCXX_DEFINES "-DGASNET_ALLOW_OPTIMIZED_DEBUG")

  add_library(upcxx STATIC IMPORTED)
  else()
set(UPCXX_NAME upcxx)
  set(UPCXX_REPO https://bitbucket.org/upcxx/upcxx.git)

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    #    set(UPCXX_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUPCXX_DEBUG")
    set(UPCXX_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
  ExternalProject_Add(${UPCXX_NAME}
      DEPENDS ${GASNET_NAME}
      GIT_REPOSITORY ${UPCXX_REPO}
      UPDATE_COMMAND ""
      INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/upcxx_install
      CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-gasnet=${GASNET_CONDUIT} CC=${MPI_C_COMPILER} CXX=${MPI_CXX_COMPILER} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${UPCXX_CXX_FLAGS}
      )
  else()
  ExternalProject_Add(${UPCXX_NAME}
      DEPENDS ${GASNET_NAME}
      GIT_REPOSITORY ${UPCXX_REPO}
      UPDATE_COMMAND ""
      INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/upcxx_install
      CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-gasnet=${GASNET_CONDUIT} CC=${MPI_C_COMPILER} CXX=${MPI_CXX_COMPILER} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${CMAKE_CXX_FLAGS}
      )
  endif()

  ExternalProject_Add_Step(${UPCXX_NAME} bootstrap
      DEPENDEES patch update patch download
      DEPENDERS configure
      COMMAND libtoolize COMMAND autoreconf -fi COMMAND <SOURCE_DIR>/Bootstrap.sh 
      WORKING_DIRECTORY <SOURCE_DIR>
#ALWAYS 1
      COMMENT "Bootstraping the source directory"
      )


set(UPCXX_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/external/upcxx_install/include)
set(UPCXX_LIBRARY_PATH ${CMAKE_CURRENT_BINARY_DIR}/external/upcxx_install/lib)


##comming from the set var.sh upc++ script
#ExternalProject_Get_Property(${GASNET_NAME} install_dir)
#set(GASNET_PREFIX ${install_dir})
#set(GASNET_LD_OVERRIDE ${CMAKE_C_COMPILER})
#set(GASNET_LDFLAGS_OVERRIDE "")
#set(GASNET_LD_OVERRIDE ${MPI_C_COMPILER})
#set(GASNET_LDFLAGS_OVERRIDE "-wd177 -wd279 -wd1572 ")
#set(CONDUIT_LIBS "-lammpi")
#set(GASNET_INCLUDES "-I${GASNET_PREFIX}/include -I${GASNET_PREFIX}/include/mpi-conduit ${CONDUIT_INCLUDES} ${CONDUIT_INCLUDES_SEQ}")
#set(GASNET_LIBDIRS "-L${GASNET_PREFIX}/lib ${CONDUIT_LIBDIRS} ${CONDUIT_LIBDIRS_SEQ}")
#set(GASNET_CC ${CMAKE_C_COMPILER}))
#set(GASNET_OPT_CFLAGS "-O3 ${CONDUIT_OPT_CFLAGS} ${CONDUIT_OPT_CFLAGS_SEQ}")
#set(GASNET_MISC_CFLAGS "-wd177 -wd279 -wd1572 ${CONDUIT_MISC_CFLAGS} ${CONDUIT_MISC_CFLAGS_SEQ}")
#set(GASNET_MISC_CPPFLAGS "${CONDUIT_MISC_CPPFLAGS} ${CONDUIT_MISC_CPPFLAGS_SEQ}")
#set(GASNET_CXX ${CMAKE_CXX_COMPILER}))
#set(GASNET_OPT_CXXFLAGS "-O2 ${CONDUIT_OPT_CFLAGS} ${CONDUIT_OPT_CFLAGS_SEQ}")
#set(GASNET_MISC_CXXFLAGS "-wd654 -wd1125 -wd279 -wd1572 ${CONDUIT_MISC_CFLAGS} ${CONDUIT_MISC_CFLAGS_SEQ}")
#set(GASNET_MISC_CXXCPPFLAGS "${CONDUIT_MISC_CPPFLAGS} ${CONDUIT_MISC_CPPFLAGS_SEQ}")
#set(GASNET_EXTRADEFINES_SEQ "")
#set(GASNET_EXTRADEFINES_PAR "-D_REENTRANT -D_GNU_SOURCE")
#set(GASNET_EXTRADEFINES_PARSYNC "-D_REENTRANT -D_GNU_SOURCE")
#set(GASNET_DEFINES "-DGASNET_SEQ ${GASNET_EXTRADEFINES_SEQ} ${CONDUIT_DEFINES} ${CONDUIT_DEFINES_SEQ}")
#set(GASNET_CFLAGS "${GASNET_OPT_CFLAGS} ${GASNET_MISC_CFLAGS} ${MANUAL_CFLAGS}")
#set(GASNET_CPPFLAGS "${GASNET_MISC_CPPFLAGS} ${GASNET_DEFINES} ${GASNET_INCLUDES}")
#set(GASNET_CXXFLAGS "${GASNET_OPT_CXXFLAGS} ${GASNET_MISC_CXXFLAGS} ${MANUAL_CXXFLAGS}")
#set(GASNET_CXXCPPFLAGS "${GASNET_MISC_CXXCPPFLAGS} ${GASNET_DEFINES} ${GASNET_INCLUDES}")
#set(GASNET_LD "${GASNET_LD_OVERRIDE}")
#set(GASNET_LDFLAGS "${GASNET_LDFLAGS_OVERRIDE} ${CONDUIT_LDFLAGS} ${CONDUIT_LDFLAGS_SEQ} ${MANUAL_LDFLAGS}")
#set(GASNET_EXTRALIBS_SEQ "")
#set(GASNET_EXTRALIBS_PAR "-lpthread")
#set(GASNET_EXTRALIBS_PARSYNC "-lpthread")
#set(GASNET_LIBS "${GASNET_LIBDIRS} -lgasnet-mpi-seq ${CONDUIT_LIBS} ${CONDUIT_LIBS_SEQ} ${GASNET_EXTRALIBS_SEQ} -lrt -lm ${MANUAL_LIBS}")
##set(GASNET_LIBS "${GASNET_LIBDIRS} -lgasnet-mpi-seq ${CONDUIT_LIBS} ${CONDUIT_LIBS_SEQ} ${GASNET_EXTRALIBS_SEQ} -lrt -lm ${MANUAL_LIBS}")
#
#
#ExternalProject_Get_Property(${UPCXX_NAME} install_dir)
#set(UPCXX_PREFIX ${install_dir})
#set(UPCXX_CXXFLAGS "${GASNET_CPPFLAGS} -I${UPCXX_PREFIX}/include -DGASNET_ALLOW_OPTIMIZED_DEBUG -std=c++11")
#set(UPCXX_LDLIBS "-L${UPCXX_PREFIX}/lib ${UPCXX_PREFIX}/lib/libupcxx.a ${GASNET_LIBS}")
#set(UPCXX_LDFLAGS ${GASNET_LDFLAGS})
#set(UPCXX_DIR ${UPCXX_PREFIX})
#
#set(GASNET_LIBRARIES "-lgasnet-mpi-seq ${CONDUIT_LIBS} ${CONDUIT_LIBS_SEQ} ${GASNET_EXTRALIBS_SEQ} -lrt -lm ${MANUAL_LIBS}")
#set(GASNET_LIBRARY_PATH "${GASNET_LIBDIRS}")
#
#set(UPCXX_LIBRARIES "${UPCXX_PREFIX}/lib/libupcxx.a")
#set(UPCXX_LIBRARY_PATH "${UPCXX_PREFIX}/lib")


set(UPCXX_LIBRARIES ${GASNET_LIBRARIES})
set(UPCXX_INCLUDE_DIR ${UPCXX_INCLUDE_DIR} ${GASNET_INCLUDE_DIR} ${GASNET_CONDUIT_INCLUDE_DIR})
set(UPCXX_LIBRARY_PATH ${UPCXX_LIBRARY_PATH} ${GASNET_LIBRARY_PATH})
set(UPCXX_DEFINES)


ExternalProject_Get_Property(${UPCXX_NAME} install_dir)
  add_library(libupcxx STATIC IMPORTED)
set_property(TARGET libupcxx PROPERTY IMPORTED_LOCATION ${install_dir}/lib/libupcxx.a)
add_dependencies(libupcxx ${UPCXX_NAME} ${GASNET_NAME})

endif()

