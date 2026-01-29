cmake_minimum_required(VERSION 3.10)
project(cxx-pm)

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
option(FULL_BUILD "Build additional tools" OFF)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/CXXPM_VERSION CXXPM_VERSION)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cxx-pm-config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cxx-pm-config.h
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/include
  ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/src
  ${CMAKE_CURRENT_SOURCE_DIR}/zstd
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
  sha3.c
  sha256.c
  base64.c
  os.cpp
  sha3Tools.cpp
  ecdsa.cpp
  version.cpp
  secp256k1/src/secp256k1.c
  secp256k1/src/precomputed_ecmult.c
  secp256k1/src/precomputed_ecmult_gen.c
  compilers/common.cpp
  compilers/gnu.cpp
  bs/autotools.cpp
  bs/cmake.cpp
  json/json11.cpp
  zstd/zstddeclib.c
  tar.cpp
  msys2db.cpp
  httpdownload.cpp
  ${SOURCES}
)

if (NOT MSVC)
  target_link_libraries(cxx-pm pthread)
endif()

if (WIN32)
  target_link_libraries(cxx-pm winhttp)
endif()

if (FULL_BUILD)
  add_executable(cxx-pm-manifestupdate
    manifestupdate.cpp
    sha3Tools.cpp
    strExtras.cpp
    sha3.c
  base64.c
    ecdsa.cpp
    secp256k1/src/secp256k1.c
    secp256k1/src/precomputed_ecmult.c
    secp256k1/src/precomputed_ecmult_gen.c
  )
endif()


install(TARGETS cxx-pm DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/add.cmake DESTINATION .)
