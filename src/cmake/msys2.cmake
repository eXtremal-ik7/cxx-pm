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
set(MSYS_RUNTIME_URL https://mirror.msys2.org/msys/x86_64/msys2-runtime-3.5.4-7-x86_64.pkg.tar.zst)
set(MSYS_RUNTIME_SHA256 880ba97fa9716e6ae0aee0ad4b4eff6ed3930676bb6a5d1bea68bfb7338eb950)
# bash
set(BASH_URL https://mirror.msys2.org/msys/x86_64/bash-5.2.037-1-x86_64.pkg.tar.zst)
set(BASH_SHA256 1d0f94a531754b34c211f727bd14fb3fe94bf87fc26719ff2e98a0022346bb7c)
# gmp
set(GMP_URL https://mirror.msys2.org/msys/x86_64/gmp-6.3.0-1-x86_64.pkg.tar.zst)
set(GMP_SHA256 d52634e1b0b99237e3ea457e5b4b114fdf39576d85efef3acb6c22069533b611)
# gcc-libs
set(GCC_LIBS_URL https://mirror.msys2.org/msys/x86_64/gcc-libs-13.3.0-1-x86_64.pkg.tar.zst)
set(GCC_LIBS_SHA256 38b6cd7df7476ce8e1b095f8f5901f93427fa901135501231295263ba171a36d)
# libintl
set(LIBINTL_URL https://mirror.msys2.org/msys/x86_64/libintl-0.22.4-1-x86_64.pkg.tar.zst)
set(LIBINTL_SHA256 4d7efc165142e31498accbe52e5bae9bdba6ff9496aab72d78d66d22c0fe1185)
# libiconv
set(LIBICONV_URL https://mirror.msys2.org/msys/x86_64/libiconv-1.17-1-x86_64.pkg.tar.zst)
set(LIBICONV_SHA256 e7f76e543fc27b44b243d9c3a69af036f40c6da64f16488813301a32646127c2)
# coreutils
set(COREUTILS_URL https://mirror.msys2.org/msys/x86_64/coreutils-8.32-5-x86_64.pkg.tar.zst)
set(COREUTILS_SHA256 62dfee1c39fd15f99c39802b35e82643bc14fffc16d6c76d4001caa385ec77e3)
# gzip
set(GZIP_URL https://mirror.msys2.org/msys/x86_64/gzip-1.13-1-x86_64.pkg.tar.zst)
set(GZIP_SHA256 5f2fced435165f16a65ae82c6877a57d693f69e48916b842f6a4ef51c3e3f4a2)
# libbz2
set(LIBBZ2_URL https://mirror.msys2.org/msys/x86_64/libbz2-1.0.8-4-x86_64.pkg.tar.zst)
set(LIBBZ2_SHA256 89fb0e1478b22b80effda55ab4ae7549388d01245d3841a096d8a2d63236c2a1)
# bzip2
set(BZIP2_URL https://mirror.msys2.org/msys/x86_64/bzip2-1.0.8-4-x86_64.pkg.tar.zst)
set(BZIP2_SHA256 635c483775de60663bd38859bb4624e6aaa1086354acf8e36513b75c24f64b3c)
# tar
set(TAR_URL https://mirror.msys2.org/msys/x86_64/tar-1.35-2-x86_64.pkg.tar.zst)
set(TAR_SHA256 1bfdfbbf51b45f53cf6b5f6f4155fc0a8ede1d740fde7fec5764f043accd76e4)
# libgpgme
set(LIBGPGME_URL https://mirror.msys2.org/msys/x86_64/libgpgme-1.24.0-1-x86_64.pkg.tar.zst)
set(LIBGPGME_SHA256 89bd67accc3875fccf5038bc25c6cb19913c1745588606c9ae718f2183cc680f)
# libunistring
set(LIBUNISTRING_URL https://mirror.msys2.org/msys/x86_64/libunistring-1.2-1-x86_64.pkg.tar.zst)
set(LIBUNISTRING_SHA256 960ca338e4c9dc625c56f65068920e48eb94997edb4751c246e4831b8226d05e)
# libidn2
set(LIBIDN2_URL https://mirror.msys2.org/msys/x86_64/libidn2-2.3.7-1-x86_64.pkg.tar.zst)
set(LIBIDN2_SHA256 73a67a8f157fad06ee079ee7d99653267fff6c72b46cae2f9a8e2ecd957dd68d)
# libpcre
set(LIBPCRE_URL https://mirror.msys2.org/msys/x86_64/libpcre2_8-10.44-1-x86_64.pkg.tar.zst)
set(LIBRCRE_SHA256 a8d657961d7b49fe6e406a18d9b5728d6ecedf3a62776178602a7defb3627e64)
# libpsl
set(LIBPSL_URL https://mirror.msys2.org/msys/x86_64/libpsl-0.21.5-2-x86_64.pkg.tar.zst)
set(LIBPSL_SHA256 26f9ba0969243231d2b9c0a7c181052b78494aaf6cc58cd59ee3064c72f80d6b)
# libutil-linux
set(LIBUTIL_URL https://mirror.msys2.org/msys/x86_64/libutil-linux-2.35.2-4-x86_64.pkg.tar.zst)
set(LIBUTIL_SHA256 26863895abfcfbd03b291ca09146cb66c207d51af7b2caef4f2ae0f8f995592a)
# libopenssl
set(LIBOPENSSL_URL https://mirror.msys2.org/msys/x86_64/libopenssl-3.4.0-1-x86_64.pkg.tar.zst)
set(LIBOPENSSL_SHA256 d882c3a05a32e1d492f682bd82ffe983410844fca156cab7f4c7fa9066ee2128)
# zlib
set(ZLIB_URL https://mirror.msys2.org/msys/x86_64/zlib-1.3.1-1-x86_64.pkg.tar.zst)
set(ZLIB_SHA256 8f027ac1b9404196a146efbf46ab12edf7fa20c7d9f6ca500d5cd275e492ef30)
# libffi
set(LIBFFI_URL https://mirror.msys2.org/msys/x86_64/libffi-3.4.6-1-x86_64.pkg.tar.zst)
set(LIBFFI_SHA256 88932fe3e7c7547830df2e345d3848e0aa039a1e84f9c96716a547224ebfe196)
# libtasn
set(LIBTASN_URL https://mirror.msys2.org/msys/x86_64/libtasn1-4.19.0-1-x86_64.pkg.tar.zst)
set(LIBTASN_SHA256 12a64edc286b731f13a745dc436f7db3a42181caa1e84baa5e300b42710d053f)
# libp11-kit
set(LIBP11_KIT_URL https://mirror.msys2.org/msys/x86_64/libp11-kit-0.25.5-2-x86_64.pkg.tar.zst)
set(LIBP11_KIT_SHA256 26ea69da9e00e989c2fe43b74e53a2168db28d970c12d8af92148ae930b223ff)
# p11-kit
set(P11_KIT_URL https://mirror.msys2.org/msys/x86_64/p11-kit-0.25.5-2-x86_64.pkg.tar.zst)
set(P11_KIT_SHA256 6dcf222a28ec2528c8dd0b1f069993a966a5fecb9c4f8a64b7390df700a26020)
# ca-certificates
set(CA_CERTIFICATES_URL https://mirror.msys2.org/msys/x86_64/ca-certificates-20240203-2-any.pkg.tar.zst)
set(CA_CERTIFICATES_SHA256 d50ff51bb4720fae07ca36c3ea7f3fca73c17ecea9fa73f199719a9c3d03accb)
# wget
set(WGET_URL https://mirror.msys2.org/msys/x86_64/wget-1.25.0-1-x86_64.pkg.tar.zst)
set(WGET_SHA256 dd75ba6f4f8fd0da4cc0729b7ff5ee959034fd2652e181cca0f31dcddb811151)
# libzstd
set(LIBZSTD_URL https://mirror.msys2.org/msys/x86_64/libzstd-1.5.6-1-x86_64.pkg.tar.zst)
set(LIBZSTD_SHA256 4d732b09013d1c3d0bb20408e2bdfb21877e50e4523cfdf220225a469ab71db9)
# zstd
set(ZSTD_URL https://mirror.msys2.org/msys/x86_64/zstd-1.5.6-1-x86_64.pkg.tar.zst)
set(ZSTD_SHA256 8534243fdf48a53fa84fe4d3fc8804a7b851199fa2931f4367765dad0d2b0e9e)
# libbz2
set(LIBBZ2_URL https://mirror.msys2.org/msys/x86_64/libbz2-1.0.8-4-x86_64.pkg.tar.zst)
set(LIBBZ2_SHA256 89fb0e1478b22b80effda55ab4ae7549388d01245d3841a096d8a2d63236c2a1)
# unzip
set(UNZIP_URL https://mirror.msys2.org/msys/x86_64/unzip-6.0-3-x86_64.pkg.tar.zst)
set(UNZIP_SHA256 c98ebac31ea92a63cf61c6190ed3e8284ccc0c29c43973f1b2c0de2874e5acfe)
# libcrypt
set(LIBCRYPT_URL https://mirror.msys2.org/msys/x86_64/libcrypt-2.1-5-x86_64.pkg.tar.zst)
set(LIBCRYPT_SHA256 3e7822d392eed281cd6019b80078ab201eefa7c83fff2d5bab0491b01088f674)
# perl
set(PERL_URL https://mirror.msys2.org/msys/x86_64/perl-5.38.2-2-x86_64.pkg.tar.zst)
set(PERL_SHA256 c3e6db01604e157665aed604c5418f4753f116ddd591c041cfd971af235b83eb)
# patch
set(PATCH_URL https://mirror.msys2.org/msys/x86_64/patch-2.7.6-2-x86_64.pkg.tar.zst)
set(PATCH_SHA256 6b1072509dc031c961b7984660b51a043186bb8976ee0b2aeeb08a78926962ef)
# brotli
set(BROTLI_URL https://mirror.msys2.org/msys/x86_64/brotli-1.1.0-1-x86_64.pkg.tar.zst)
set(BROTLI_SHA256 93dfc7b33c177404f8991e29e11b8f8594c0e59625e09d8c61ef6bbe91e8f9b6)
# libnghttp2
set(LIBNGHTTP2_URL https://mirror.msys2.org/msys/x86_64/libnghttp2-1.64.0-1-x86_64.pkg.tar.zst)
set(LIBNGHTTP2_SHA256 a7ed3f57400375b008ef4662b0ac8ae6f163dff5198e87aaaba5a84c69d22f44)
# sqlite
set(SQLITE_URL https://mirror.msys2.org/msys/x86_64/libsqlite-3.46.1-1-x86_64.pkg.tar.zst)
set(SQLITE_SHA256 e0b80f0426d8cee0114f37f7579725a2117bb4dd9ffb0a7d94ee6610cfbd162d)
# heimdal-libs
set(HEIMDAL_URL https://mirror.msys2.org/msys/x86_64/heimdal-libs-7.8.0-5-x86_64.pkg.tar.zst)
set(HEIMDAL_SHA256 d5f139b6feebd75f0b5fefec952c7fbb4cdfa16235601c223dabc3b1bcead19f)
# libssh2
set(LIBSSH2_URL https://mirror.msys2.org/msys/x86_64/libssh2-1.11.0-1-x86_64.pkg.tar.zst)
set(LIBSSH2_SHA256 8535b9757ff44edab022133eb54430908ac51c710bc9c7f18a09370cc4cc8421)
# libcurl
set(LIBCURL_URL https://mirror.msys2.org/msys/x86_64/libcurl-8.9.1-1-x86_64.pkg.tar.zst)
set(LIBCURL_SHA256 3fa6d0fd9e9ee8e0b0b1b2dbf65bd53878c9906539d732a45a6a8d05ac460cd2)
# git
set(GIT_URL https://mirror.msys2.org/msys/x86_64/git-2.47.0-1-x86_64.pkg.tar.zst)
set(GIT_SHA256 03cccc6c91ef64dc97c88a8245643ec0702b693bedd388de22f95c44d7988b71)

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
