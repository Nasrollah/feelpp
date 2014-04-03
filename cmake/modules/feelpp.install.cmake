###  coding: utf-8 ---

#  Author(s): Christophe Prud'homme <prudhomme@unistra.fr>
#       Date: 2013-02-08
#
#  Copyright (C) 2013 Universit� de Strasbourg
#
# Distributed under the GPL(GNU Public License):
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#
set(INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(FEELPP_PREFIX ${CMAKE_INSTALL_PREFIX})
if (NOT FEELPP_DATADIR )
  set(FEELPP_DATADIR ${CMAKE_INSTALL_PREFIX}/share/feel )
endif()
CONFIGURE_FILE(feelconfig.h.in feel/feelconfig.h  @ONLY)
CONFIGURE_FILE(feelinfo.h.in feel/feelinfo.h  @ONLY)

#
# Packaging
#
INCLUDE(InstallRequiredSystemLibraries)
feelpp_list_subdirs(feeldirs ${CMAKE_CURRENT_SOURCE_DIR}/feel)

foreach(includedir ${feeldirs})
  FILE(GLOB files "feel/${includedir}/*.hpp" )
  FILE(GLOB cppfiles "feel/${includedir}/*.cpp" )
  INSTALL(FILES ${files} ${cppfiles} DESTINATION include/feel/${includedir} COMPONENT Devel)
  FILE(GLOB details "feel/${includedir}/detail/*.hpp")
  INSTALL(FILES ${details} DESTINATION include/feel/${includedir}/detail COMPONENT Devel)
endforeach()
FILE(GLOB files "feel/*.hpp")
INSTALL(FILES ${files} DESTINATION include/feel COMPONENT Devel)

# install cln headers
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/cln/include/cln/*.h")
INSTALL(FILES ${files} DESTINATION include/feel/cln COMPONENT Devel)
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/cln/lib/lib*" "${CMAKE_CURRENT_BINARY_DIR}/contrib/cln/lib64/lib*")
INSTALL(FILES ${files} DESTINATION lib/ COMPONENT Devel)

# install gflags headers
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/gflags/include/gflags/*.h")
INSTALL(FILES ${files} DESTINATION include/feel/gflags COMPONENT Devel)
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/gflags/lib/lib*" "${CMAKE_CURRENT_BINARY_DIR}/contrib/gflags/lib64/lib*")
INSTALL(FILES ${files} DESTINATION lib/ COMPONENT Devel)

# install glog headers
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/glog/include/glog/*.h")
INSTALL(FILES ${files} DESTINATION include/feel/glog COMPONENT Devel)
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/glog/lib/lib*" "${CMAKE_CURRENT_BINARY_DIR}/contrib/glog/lib64/lib*")
INSTALL(FILES ${files} DESTINATION lib/ COMPONENT Devel)

# install matheval headers
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/libmatheval/include/*.h")
INSTALL(FILES ${files} DESTINATION include/feel/matheval COMPONENT Devel)
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/contrib/libmatheval/lib/lib*" "${CMAKE_CURRENT_BINARY_DIR}/contrib/libmatheval/lib64/lib*")
INSTALL(FILES ${files} DESTINATION lib/ COMPONENT Devel)


# # gmm
# IF ( NOT GMM_FOUND )
#   FILE(GLOB files "contrib/gmm/include/*.h")
#   INSTALL(FILES ${files} DESTINATION include/feel COMPONENT Devel)
# ENDIF()

# feel++ config headers
FILE(GLOB files "${CMAKE_CURRENT_BINARY_DIR}/feel/*.h")
INSTALL(FILES ${files} DESTINATION include/feel COMPONENT Devel)


FILE(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/applications/crb/templates/*")
INSTALL(FILES ${files} DESTINATION share/feel/crb/templates COMPONENT Devel)

# documentation and examples
#  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/feel_get_tutorial.sh DESTINATION bin COMPONENT Doc)
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/manual/feelpp-manual.pdf DESTINATION share/doc/feel COMPONENT Doc)
IF( EXISTS "${CMAKE_CURRENT_BINARY_DIR}/doc/api/html" )
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/api/html DESTINATION share/doc/feel COMPONENT Doc
    PATTERN ".svn" EXCLUDE PATTERN ".git" EXCLUDE )
ENDIF()

FILE(GLOB files "quickstart/qs_*")
INSTALL(FILES ${files} DESTINATION share/doc/feel/examples/ COMPONENT Doc)

FILE(WRITE CMakeLists.txt.doc  "cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH \"${CMAKE_INSTALL_PREFIX}/share/feel/cmake/modules/\")
Find_Package(Feel++)

add_custom_target(tutorial)

feelpp_add_application( qs_laplacian SRCS qs_laplacian.cpp INCLUDE_IN_ALL)
feelpp_add_application( qs_stokes SRCS qs_stokes.cpp INCLUDE_IN_ALL)
feelpp_add_application( qs_ns SRCS qs_ns.cpp INCLUDE_IN_ALL)
")

FILE(GLOB examples
  "${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/tutorial/*.*pp")
FILE(GLOB examplescfg
  "${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/tutorial/*.cfg"
  "${CMAKE_CURRENT_SOURCE_DIR}/doc/manual/tutorial/*.geo" )

INSTALL(FILES ${examples} DESTINATION share/doc/feel/examples/ COMPONENT Doc)
INSTALL(FILES ${examplescfg} DESTINATION share/doc/feel/examples/ COMPONENT Doc)
foreach(example ${examples} )
  get_filename_component( EXAMPLE_TARGET_NAME ${example} NAME_WE )
  get_filename_component( EXAMPLE_SRCS_NAME ${example} NAME )
  FILE(APPEND CMakeLists.txt.doc "
# target feelpp_doc_${EXAMPLE_TARGET_NAME}
feelpp_add_application( doc_${EXAMPLE_TARGET_NAME} SRCS ${EXAMPLE_SRCS_NAME} INCLUDE_IN_ALL)
" )
endforeach()
foreach( example myapp mymesh myintegrals myfunctionspace mylaplacian mystokes)
  FILE(APPEND CMakeLists.txt.doc "
add_dependencies(tutorial feelpp_doc_${example})
")
endforeach()
INSTALL(FILES CMakeLists.txt.doc DESTINATION share/doc/feel/examples/ COMPONENT Doc RENAME CMakeLists.txt)
