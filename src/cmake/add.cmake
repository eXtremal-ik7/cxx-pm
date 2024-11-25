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

    if (CMAKE_OSX_SYSROOT)
      set(CXXPM_ISYSROOT_ARG "--isysroot=${CMAKE_OSX_SYSROOT}" PARENT_SCOPE)
    endif()
  endif()
  
  
endfunction()

function(__cxxpm_install cxxpm name configuration)
  execute_process(COMMAND ${cxxpm}
    ${CXXPM_VC_INSTALL_DIR_ARG}
    ${CXXPM_VC_TOOLSET_ARG}
    ${CXXPM_ISYSROOT_ARG}
    ${CXXPM_C_COMPILER_ARG}
    ${CXXPM_CXX_COMPILER_ARG}
    "--system-name=${CXXPM_SYSTEM_NAME}"
    "--system-processor=${CXXPM_SYSTEM_PROCESSOR}"
    "--build-type=${configuration}"
    "--install=${name}"
    "--export-cmake=${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake"
    RESULT_VARIABLE EXIT_CODE
    COMMAND_ECHO STDOUT)

  if (NOT (EXIT_CODE EQUAL 0))
    message(FATAL_ERROR "cxx-pm: failed to install ${name}")
  endif()
endfunction()

function(cxxpm_add_package name)
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
  __cxxpm_install(${CXXPM} ${name} ${CONFIGURATION})
  include(${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake)
endfunction()
