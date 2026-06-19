function(__cxxpm_prepare)
  # System name: use CMAKE_SYSTEM_NAME
  set(CXXPM_SYSTEM_NAME ${CMAKE_SYSTEM_NAME} PARENT_SCOPE)
  
  # System processor
  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    if (CMAKE_VS_PLATFORM_NAME)
      set(CXXPM_SYSTEM_PROCESSOR ${CMAKE_VS_PLATFORM_NAME} PARENT_SCOPE)
    else()
      set(CXXPM_SYSTEM_PROCESSOR ${CMAKE_VS_PLATFORM_NAME_DEFAULT} PARENT_SCOPE)
    endif()
    
    # Initialize VC_INSTALL_DIR
    set(CXXPM_VC_INSTALL_DIR_ARG "--vs-install-dir=${CMAKE_GENERATOR_INSTANCE}" PARENT_SCOPE)
    if (CMAKE_GENERATOR_TOOLSET)
      set(CXXPM_VC_TOOLSET_ARG "--vc-toolset=${CMAKE_GENERATOR_TOOLSET}" PARENT_SCOPE)
    endif()
  else()
    if (CMAKE_OSX_ARCHITECTURES)
      set(CXXPM_SYSTEM_PROCESSOR ${CMAKE_OSX_ARCHITECTURES} PARENT_SCOPE)
    else()
      set(CXXPM_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR} PARENT_SCOPE)
    endif()
    
    set(CXXPM_C_COMPILER_ARG "--compiler=C:${CMAKE_C_COMPILER}" PARENT_SCOPE)
    set(CXXPM_CXX_COMPILER_ARG "--compiler=C++:${CMAKE_CXX_COMPILER}" PARENT_SCOPE)

    # Android (NDK): CMake sets CMAKE_C_COMPILER to the generic, multi-target
    # "clang" and selects the target via injected --target flags. cxx-pm instead
    # identifies a platform from the driver's own reported target, so hand it the
    # NDK's per-ABI versioned driver (e.g. aarch64-linux-android24-clang): it
    # self-reports the android triple, and package recipes can derive the NDK
    # root + API level straight from its path.
    if (CMAKE_SYSTEM_NAME STREQUAL "Android")
      if (ANDROID_TOOLCHAIN_ROOT)
        set(_cxxpm_ndk_bin "${ANDROID_TOOLCHAIN_ROOT}/bin")
      else()
        set(_cxxpm_ndk_bin "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/bin")
      endif()
      if (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        set(_cxxpm_android_triple "aarch64-linux-android")
      elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
        set(_cxxpm_android_triple "x86_64-linux-android")
      elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
        set(_cxxpm_android_triple "armv7a-linux-androideabi")
      elseif (CMAKE_ANDROID_ARCH_ABI STREQUAL "x86")
        set(_cxxpm_android_triple "i686-linux-android")
      endif()
      # Numeric API level (CMAKE_SYSTEM_VERSION is unreliable here — it stays 1;
      # the NDK toolchain exposes the parsed level as ANDROID_PLATFORM_LEVEL).
      set(_cxxpm_android_api "${ANDROID_PLATFORM_LEVEL}")
      if (NOT _cxxpm_android_api)
        set(_cxxpm_android_api "${ANDROID_NATIVE_API_LEVEL}")
      endif()
      set(_cxxpm_android_cc "${_cxxpm_ndk_bin}/${_cxxpm_android_triple}${_cxxpm_android_api}-clang")
      if (_cxxpm_android_triple AND EXISTS "${_cxxpm_android_cc}")
        set(CXXPM_C_COMPILER_ARG "--compiler=C:${_cxxpm_android_cc}" PARENT_SCOPE)
        set(CXXPM_CXX_COMPILER_ARG "--compiler=C++:${_cxxpm_android_cc}++" PARENT_SCOPE)
      endif()
    endif()

    if (CMAKE_OSX_SYSROOT)
      set(CXXPM_ISYSROOT_ARG "--isysroot=${CMAKE_OSX_SYSROOT}" PARENT_SCOPE)
    endif()
  endif()
  
  
endfunction()

function(__cxxpm_install cxxpm name version configuration)
  if (version)
    set(PACKAGE_SPEC "${name}@${version}")
  else()
    set(PACKAGE_SPEC "${name}")
  endif()

  execute_process(COMMAND ${cxxpm}
    ${CXXPM_VC_INSTALL_DIR_ARG}
    ${CXXPM_VC_TOOLSET_ARG}
    ${CXXPM_ISYSROOT_ARG}
    ${CXXPM_C_COMPILER_ARG}
    ${CXXPM_CXX_COMPILER_ARG}
    "--system-name=${CXXPM_SYSTEM_NAME}"
    "--system-processor=${CXXPM_SYSTEM_PROCESSOR}"
    "--build-type=${configuration}"
    "--install=${PACKAGE_SPEC}"
    "--export-cmake=${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake"
    RESULT_VARIABLE EXIT_CODE
    COMMAND_ECHO STDOUT)

  if (NOT (EXIT_CODE EQUAL 0))
    message(FATAL_ERROR "cxx-pm: failed to install ${name}")
  endif()
endfunction()

function(cxxpm_add_package name version)
  __cxxpm_prepare()
  get_property(CXXPM GLOBAL PROPERTY CXXPM_EXECUTABLE)

  if (CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator, use CMAKE_CONFIGURATION_TYPES
    string(REPLACE ";" "\;" CONFIGURATION "${CMAKE_CONFIGURATION_TYPES}")
  else()
    # single-config generator, use CMAKE_BUILD_TYPE
    if (CMAKE_BUILD_TYPE)
      set(CONFIGURATION ${CMAKE_BUILD_TYPE})
    else()
      set(CONFIGURATION Release)
    endif()    
  endif()

  # Install package and export cmake file
  __cxxpm_install(${CXXPM} ${name} ${version} ${CONFIGURATION})
  include(${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake)
endfunction()
