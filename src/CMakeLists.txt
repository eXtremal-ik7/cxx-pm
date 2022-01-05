project(cxx-pm)
cmake_minimum_required(VERSION 3.10)

function(__get_home_dir home)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(HOMEDRIVE $ENV{HOMEDRIVE})
    set(HOMEPATH $ENV{HOMEPATH})
    if (HOMEDRIVE AND HOMEPATH)
      file(TO_CMAKE_PATH $ENV{HOMEDRIVE}$ENV{HOMEPATH} HOME)
      set(${home} ${HOME} PARENT_SCOPE)
    else()
      set(${home} $ENV{HOME} PARENT_SCOPE)
    endif()
  else()
    set(${home} $ENV{HOME} PARENT_SCOPE)
  endif()
endfunction()

__get_home_dir(USER_HOME_DIRECTORY)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${USER_HOME_DIRECTORY}/.cxxpm/self CACHE PATH "default" FORCE)
endif()

set (CMAKE_CXX_STANDARD 17)
option(MSYS2_PACKAGE_BUILD OFF)
if (WIN32)
  set(MSYS2_PACKAGE_BUILD ON)
endif()

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/CXXPM_VERSION CXXPM_VERSION)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cxx-pm-config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cxx-pm-config.h
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (MSVC)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/msvc/include)
  set(SOURCES ${SOURCES}
    msvc/getopt.c
    msvc/getopt1.c
  )
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (WIN32)
  set(SOURCES ${SOURCES}
    compilers/msvc.cpp
  )
endif()

add_executable(cxx-pm
  main.cpp
  exec.cpp
  package.cpp
  strExtras.cpp
  tiny_sha3.c
  os.cpp
  sha3.cpp
  compilers/common.cpp
  compilers/gnu.cpp
  bs/autotools.cpp
  bs/cmake.cpp
  json/json11.cpp
  ${SOURCES}
)

if (NOT MSVC)
  target_link_libraries(cxx-pm pthread)
endif()

if (MSYS2_PACKAGE_BUILD)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/msys2.cmake)
  msys2_build()
endif()

install(TARGETS cxx-pm DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/packages DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/add.cmake DESTINATION .)
