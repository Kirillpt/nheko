set(WINDOWS_FLAGS "")

if(MSVC)
    set(WINDOWS_FLAGS "-DCMAKE_GENERATOR_PLATFORM=x64")
endif()

ExternalProject_Add(
   Maddy

   URL ${MADDY_URL}
   URL_HASH SHA256=${MADDY_HASH}

   BUILD_IN_SOURCE 1
   SOURCE_DIR ${DEPS_BUILD_DIR}/maddy
   CONFIGURE_COMMAND ${CMAKE_COMMAND}
        -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
        ${DEPS_BUILD_DIR}/maddy
        ${WINDOWS_FLAGS})

list(APPEND THIRD_PARTY_DEPS Maddy)