function(check_sha256 path hash VAR)
  set(${VAR} 0 PARENT_SCOPE)
  if (EXISTS ${path})
    file(SHA256 ${path} HASH)
    if (HASH STREQUAL hash)
      set(${VAR} 1 PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(download_file package_name url hash)
  get_filename_component(FILE_NAME ${url} NAME)
  set(PATH ${CMAKE_CURRENT_BINARY_DIR}/msys-bundle/${FILE_NAME})
  set(${package_name}_PATH ${PATH} PARENT_SCOPE)

  check_sha256(${PATH} ${hash} VALID)
  if (VALID EQUAL 1)
    message("Already have ${url}")
    return()
  endif()

  message("Downloading ${url} ...")
  file(DOWNLOAD ${url} ${PATH} SHOW_PROGRESS STATUS DOWNLOAD_STATUS)
  if (NOT (DOWNLOAD_STATUS EQUAL 0))
    message(FATAL_ERROR "Can't download ${url}")
  endif()

  check_sha256(${PATH} ${hash} VALID)
  if (VALID EQUAL 0)
    message(FATAL_ERROR "File ${PATH} does not match sha256 hash")
  endif()
endfunction(download_file)

function(install_file path)
  install(CODE "
    message(\"installing ${path} ...\")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf \"${path}\" WORKING_DIRECTORY \"${CMAKE_INSTALL_PREFIX}\")
  ")
endfunction()

# msys runtime
set(MSYS_RUNTIME_URL https://mirror.msys2.org/msys/x86_64/msys2-runtime-3.3.3-1-x86_64.pkg.tar.zst)
set(MSYS_RUNTIME_SHA256 7aadfd6ae1990989f305abba41541fa1286d74181346e96643533afa2e1377e2)
# bash
set(BASH_URL https://mirror.msys2.org/msys/x86_64/bash-5.1.008-1-x86_64.pkg.tar.zst)
set(BASH_SHA256 1111c47c37116bf461d2ab7d0e7a6fb0ee3b4a6bea362309c58a2e0ea5f198ff)
# gmp
set(GMP_URL https://mirror.msys2.org/msys/x86_64/gmp-6.2.1-1-x86_64.pkg.tar.zst)
set(GMP_SHA256 31bcec61829c0d1cdc490dc8661c48a78bf02a9dafa074411919e7fbdde53c35)
# gcc-libs
set(GCC_LIBS_URL https://mirror.msys2.org/msys/x86_64/gcc-libs-11.2.0-1-x86_64.pkg.tar.zst)
set(GCC_LIBS_SHA256 0cfd77a9ad864e2181cfe0dafb9e4a19d6ae17689df0b55f446c53736f349a61)
# libintl
set(LIBINTL_URL https://mirror.msys2.org/msys/x86_64/libintl-0.19.8.1-1-x86_64.pkg.tar.xz)
set(LIBINTL_SHA256 5eadc3cc42da78948d65d994f1f8326706afe011f28e2e5bd0872a37612072d2)
# libiconv
set(LIBICONV_URL https://mirror.msys2.org/msys/x86_64/libiconv-1.16-2-x86_64.pkg.tar.zst)
set(LIBICONV_SHA256 4d23674f25e9d558295464b4f50689698f8ce240616410da9a4d9420b5130ced)
# coreutils
set(COREUTILS_URL https://mirror.msys2.org/msys/x86_64/coreutils-8.32-2-x86_64.pkg.tar.zst)
set(COREUTILS_SHA256 af4183e8b97dd509e05ac0146859dbd4f9a686a68397c173cd973b11e7d8597f)
# gzip
set(GZIP_URL https://mirror.msys2.org/msys/x86_64/gzip-1.11-1-x86_64.pkg.tar.zst)
set(GZIP_SHA256 8215d34b82db62e95df47a46233d8e836902c1acb70308f1e200aa5f1ff40d7e)
# libbz2
set(LIBBZ2_URL https://mirror.msys2.org/msys/x86_64/libbz2-1.0.8-3-x86_64.pkg.tar.zst)
set(LIBBZ2_SHA256 617569f3a595e303f02d2323907761a13d60614d93336a7c9d4500533d52aaf1)
# bzip2
set(BZIP2_URL https://mirror.msys2.org/msys/x86_64/bzip2-1.0.8-3-x86_64.pkg.tar.zst)
set(BZIP2_SHA256 b6ebeef89baf14f397b7862554bae96885533dcb55fe38419129bf6394283113)
# tar
set(TAR_URL https://mirror.msys2.org/msys/x86_64/tar-1.34-1-x86_64.pkg.tar.zst)
set(TAR_SHA256 f10680ddfd5bb15f5de9f0b2dd4f85cf57ec3b0337a841f20f27c00838683744)
# libgpgme
set(LIBGPGME_URL https://mirror.msys2.org/msys/x86_64/libgpgme-1.16.0-1-x86_64.pkg.tar.zst)
set(LIBGPGME_SHA256 c3f85e73d5210b17609df597656ef0c9326eb282be5db0828545e43eeccd7473)
# libunistring
set(LIBUNISTRING_URL https://mirror.msys2.org/msys/x86_64/libunistring-0.9.10-1-x86_64.pkg.tar.xz)
set(LIBUNISTRING_SHA256 64ca150cf3a112dbd5876ad0d36b727f28227a3e3e4d3aec9f0cc3224a4ddaf5)
# libidn2
set(LIBIDN2_URL https://mirror.msys2.org/msys/x86_64/libidn2-2.3.2-1-x86_64.pkg.tar.zst)
set(LIBIDN2_SHA256 2b2812ddb0febe66c330e6c4ef52432d7cfaf7657423fb268e1dd9348b7ad301)
# libpcre
set(LIBPCRE_URL https://mirror.msys2.org/msys/x86_64/libpcre2_8-10.37-1-x86_64.pkg.tar.zst)
set(LIBRCRE_SHA256 8f0a6a51dca241d5f5530edd3147bbd80d1690396d1d5e1788b4e025b69f605b)
# libpsl
set(LIBPSL_URL https://mirror.msys2.org/msys/x86_64/libpsl-0.21.1-2-x86_64.pkg.tar.zst)
set(LIBPSL_SHA256 351a98020df201f746f73893c2f93ced3a10cd30968cc92ee304da6c5eba4c88)
# libutil-linux
set(LIBUTIL_URL https://mirror.msys2.org/msys/x86_64/libutil-linux-2.35.2-1-x86_64.pkg.tar.zst)
set(LIBUTIL_SHA256 cbc277391ff856a2c7eebc7b305e49b4e925df1e65ec077b4bdb8c953e5b61dc)
# libopenssl
set(LIBOPENSSL_URL https://mirror.msys2.org/msys/x86_64/libopenssl-1.1.1.l-1-x86_64.pkg.tar.zst)
set(LIBOPENSSL_SHA256 6657a7a2bef09df5368c08fe088c21402e9cb5e41a3cfe903c469f8b1d220aee)
# zlib
set(ZLIB_URL https://mirror.msys2.org/msys/x86_64/zlib-1.2.11-1-x86_64.pkg.tar.xz)
set(ZLIB_SHA256 4af63558e39e7a4941292132b2985cb2650e78168ab21157a082613215e4839a)
# libffi
set(LIBFFI_URL https://mirror.msys2.org/msys/x86_64/libffi-3.3-1-x86_64.pkg.tar.xz)
set(LIBFFI_SHA256 06e9f64dc7832498caee7d02a3e4877749319b64339268fcc8e3b7c6fb4d8580)
# libtasn
set(LIBTASN_URL https://mirror.msys2.org/msys/x86_64/libtasn1-4.18.0-1-x86_64.pkg.tar.zst)
set(LIBTASN_SHA256 675971eebc496c086213dc16dcad4226af4c7d0b39908273d34675c27eff9211)
# libp11-kit
set(LIBP11_KIT_URL https://mirror.msys2.org/msys/x86_64/libp11-kit-0.24.0-1-x86_64.pkg.tar.zst)
set(LIBP11_KIT_SHA256 29903059025589386a6dcbefe5e1cf8c610d60760c4f3d4053f414751a5b7ab7)
# p11-kit
set(P11_KIT_URL https://mirror.msys2.org/msys/x86_64/p11-kit-0.24.0-1-x86_64.pkg.tar.zst)
set(P11_KIT_SHA256 6bf4550b6bc93b5039a66e2838db7175e28853197bb97c1c30266efe8683b794)
# ca-certificates
set(CA_CERTIFICATES_URL https://mirror.msys2.org/msys/x86_64/ca-certificates-20210119-3-any.pkg.tar.zst)
set(CA_CERTIFICATES_SHA256 3a3ab89ba7cf3ac7f2d6bfbdf252849c1f23f8566db620b9ded296e935fd6d43)
# wget
set(WGET_URL https://mirror.msys2.org/msys/x86_64/wget-1.21.2-1-x86_64.pkg.tar.zst)
set(WGET_SHA256 118b5fe59a8b56b3d2eab5b2bb1fc2ab8b3c79742c2fd72c089e76c6559e0207)
# libzstd
set(LIBZSTD_URL https://mirror.msys2.org/msys/x86_64/libzstd-1.5.0-1-x86_64.pkg.tar.zst)
set(LIBZSTD_SHA256 8f2eba12c58108092ae429e37bd44bfca26e08b3684e092583b4e3bab42582b4)
# zstd
set(ZSTD_URL https://mirror.msys2.org/msys/x86_64/zstd-1.5.0-1-x86_64.pkg.tar.zst)
set(ZSTD_SHA256 2505834b7c3d1064ba005dbf6b1fcf68782191544c015de9c810e97f2043f59a)
# libbz2
set(LIBBZ2_URL https://mirror.msys2.org/msys/x86_64/libbz2-1.0.8-2-x86_64.pkg.tar.xz)
set(LIBBZ2_SHA256 f31f91c0649ff1cd2292fb79ce19d818a43ac40039bb2cfaf94195ad52fed8bc)
# unzip
set(UNZIP_URL https://mirror.msys2.org/msys/x86_64/unzip-6.0-2-x86_64.pkg.tar.xz)
set(UNZIP_SHA256 8594ccda17711c5fad21ebb0e09ce37452cdf78803ca0b7ffbab1bdae1aa170c)
# libcrypt
set(LIBCRYPT_URL https://mirror.msys2.org/msys/x86_64/libcrypt-2.1-3-x86_64.pkg.tar.zst)
set(LIBCRYPT_SHA256 2ed4842520063192fc09ebc50d150e53b1f2224485833fa89582789ee462d8ac)
# perl
set(PERL_URL https://mirror.msys2.org/msys/x86_64/perl-5.32.1-1-x86_64.pkg.tar.zst)
set(PERL_SHA256 1fe9841c06e0a375fdf60cc41d31a0bf5fd764b4eab986751b0424dd50484970)
# patch
set(PATCH_URL https://mirror.msys2.org/msys/x86_64/patch-2.7.6-1-x86_64.pkg.tar.xz)
set(PATCH_SHA256 5c18ce8979e9019d24abd2aee7ddcdf8824e31c4c7e162a204d4dc39b3b73776)
# brotli
set(BROTLI_URL https://mirror.msys2.org/msys/x86_64/brotli-1.0.9-2-x86_64.pkg.tar.zst)
set(BROTLI_SHA256 7286fc22de0a0d454cbe6be77851d8a9d494c76f7e139829b9469cf6c9c86ab2)
# libnghttp2
set(LIBNGHTTP2_URL https://mirror.msys2.org/msys/x86_64/libnghttp2-1.46.0-1-x86_64.pkg.tar.zst)
set(LIBNGHTTP2_SHA256 509900f72847eb37be874d4670beb7e2594db065a65c0f8f16142f83e765a842)
# sqlite
set(SQLITE_URL https://mirror.msys2.org/msys/x86_64/libsqlite-3.36.0-3-x86_64.pkg.tar.zst)
set(SQLITE_SHA256 a18e525f4c185576aaf6e314213cc805dbac81846e4cb34ce4c4d78f18280561)
# heimdal-libs
set(HEIMDAL_URL https://mirror.msys2.org/msys/x86_64/heimdal-libs-7.7.0-3-x86_64.pkg.tar.zst)
set(HEIMDAL_SHA256 adfc7d3a7eaa8c66696c3f3543d82a9056188caa578ddaf56abd8724d131c590)
# libssh2
set(LIBSSH2_URL https://mirror.msys2.org/msys/x86_64/libssh2-1.10.0-1-x86_64.pkg.tar.zst)
set(LIBSSH2_SHA256 452ed7ae24f0d353bf3aa3918c08f70470af8f11cc7b3f4cbf0993aea7f63f47)
# libcurl
set(LIBCURL_URL https://mirror.msys2.org/msys/x86_64/libcurl-7.80.0-3-x86_64.pkg.tar.zst)
set(LIBCURL_SHA256 6f7e018101bf54f7fa45d5c1b1c0caa20ae93c2f97e9e968a5793879e9b14d0f)
# git
set(GIT_URL https://mirror.msys2.org/msys/x86_64/git-2.34.1-1-x86_64.pkg.tar.zst)
set(GIT_SHA256 5925228c1e9e51a7fa548236a0376b6d0e90766cc7251668aa589a80afb770b1)

function (msys2_build)
  download_file(msys2_msys_runtime ${MSYS_RUNTIME_URL} ${MSYS_RUNTIME_SHA256})
  download_file(msys2_bash ${BASH_URL} ${BASH_SHA256})
  download_file(msys2_gmp ${GMP_URL} ${GMP_SHA256})
  download_file(msys2_gcc_libs ${GCC_LIBS_URL} ${GCC_LIBS_SHA256})
  download_file(msys2_libintl ${LIBINTL_URL} ${LIBINTL_SHA256})
  download_file(msys2_libiconv ${LIBICONV_URL} ${LIBICONV_SHA256})
  download_file(msys2_coreutils ${COREUTILS_URL} ${COREUTILS_SHA256})
  download_file(msys2_gzip ${GZIP_URL} ${GZIP_SHA256})
  download_file(msys2_libbz2 ${LIBBZ2_URL} ${LIBBZ2_SHA256})
  download_file(msys2_bzip2 ${BZIP2_URL} ${BZIP2_SHA256})
  download_file(msys2_tar ${TAR_URL} ${TAR_SHA256})
  download_file(msys2_libunistring ${LIBUNISTRING_URL} ${LIBUNISTRING_SHA256})
  download_file(msys2_libidn2 ${LIBIDN2_URL} ${LIBIDN2_SHA256})
  download_file(msys2_libpcre ${LIBPCRE_URL} ${LIBRCRE_SHA256})
  download_file(msys2_libpsl ${LIBPSL_URL} ${LIBPSL_SHA256})
  download_file(msys2_libutil ${LIBUTIL_URL} ${LIBUTIL_SHA256})
  download_file(msys2_libopenssl ${LIBOPENSSL_URL} ${LIBOPENSSL_SHA256})
  download_file(msys2_zlib ${ZLIB_URL} ${ZLIB_SHA256})
  download_file(msys2_libffi ${LIBFFI_URL} ${LIBFFI_SHA256})
  download_file(msys2_libtasn ${LIBTASN_URL} ${LIBTASN_SHA256})
  download_file(msys2_libp11_kit ${LIBP11_KIT_URL} ${LIBP11_KIT_SHA256})
  download_file(msys2_p11_kit ${P11_KIT_URL} ${P11_KIT_SHA256})
  download_file(msys2_ca_certificates ${CA_CERTIFICATES_URL} ${CA_CERTIFICATES_SHA256})
  download_file(msys2_wget ${WGET_URL} ${WGET_SHA256})
  download_file(msys2_libzstd ${LIBZSTD_URL} ${LIBZSTD_SHA256})
  download_file(msys2_zstd ${ZSTD_URL} ${ZSTD_SHA256})
  download_file(msys2_unzip ${UNZIP_URL} ${UNZIP_SHA256})
  download_file(msys2_libcrypt ${LIBCRYPT_URL} ${LIBCRYPT_SHA256})
  download_file(msys2_perl ${PERL_URL} ${PERL_SHA256})
  download_file(msys2_patch ${PATCH_URL} ${PATCH_SHA256})
  download_file(msys2_brotli ${BROTLI_URL} ${BROTLI_SHA256})
  download_file(msys2_libnghttp2 ${LIBNGHTTP2_URL} ${LIBNGHTTP2_SHA256})
  download_file(msys2_sqlite ${SQLITE_URL} ${SQLITE_SHA256})
  download_file(msys2_heimdal ${HEIMDAL_URL} ${HEIMDAL_SHA256})
  download_file(msys2_libssh2 ${LIBSSH2_URL} ${LIBSSH2_SHA256})
  download_file(msys2_libcurl ${LIBCURL_URL} ${LIBCURL_SHA256})
  download_file(msys2_git ${GIT_URL} ${GIT_SHA256})

  install(CODE "
    file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX})
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/usr)
    file(REMOVE_RECURSE ${CMAKE_INSTALL_PREFIX}/etc)
  ")

  install_file(${msys2_msys_runtime_PATH})
  install_file(${msys2_bash_PATH})
  install_file(${msys2_gmp_PATH})
  install_file(${msys2_gcc_libs_PATH})
  install_file(${msys2_libintl_PATH})
  install_file(${msys2_libiconv_PATH})
  install_file(${msys2_coreutils_PATH})
  install_file(${msys2_gzip_PATH})
  install_file(${msys2_libbz2_PATH})
  install_file(${msys2_bzip2_PATH})
  install_file(${msys2_tar_PATH})
  install_file(${msys2_libunistring_PATH})
  install_file(${msys2_libidn2_PATH})
  install_file(${msys2_libpcre_PATH})
  install_file(${msys2_libpsl_PATH})
  install_file(${msys2_libutil_PATH})
  install_file(${msys2_libopenssl_PATH})
  install_file(${msys2_zlib_PATH})
  install_file(${msys2_libffi_PATH})
  install_file(${msys2_libtasn_PATH})
  install_file(${msys2_libp11_kit_PATH})
  install_file(${msys2_p11_kit_PATH})
  install_file(${msys2_ca_certificates_PATH})
  install_file(${msys2_wget_PATH})
  install_file(${msys2_libzstd_PATH})
  install_file(${msys2_zstd_PATH})
  install_file(${msys2_unzip_PATH})
  install_file(${msys2_libcrypt_PATH})
  install_file(${msys2_perl_PATH})
  install_file(${msys2_patch_PATH})
  install_file(${msys2_brotli_PATH})
  install_file(${msys2_libnghttp2_PATH})
  install_file(${msys2_sqlite_PATH})
  install_file(${msys2_heimdal_PATH})
  install_file(${msys2_libssh2_PATH})
  install_file(${msys2_libcurl_PATH})
  install_file(${msys2_git_PATH})
  
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cmake/etc DESTINATION .)

  install(DIRECTORY DESTINATION tmp)

  install(CODE "
    set(ENV{PATH} \"ENV{PATH};${CMAKE_INSTALL_PREFIX}/usr/bin\")
    execute_process(COMMAND ${CMAKE_INSTALL_PREFIX}/usr/bin/bash.exe update-ca-trust WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/usr/bin)
  ")
endfunction()
