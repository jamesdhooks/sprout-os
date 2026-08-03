set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(SPROUT_ONION_TOOLCHAIN_ROOT "/opt/miyoomini-toolchain")
set(CMAKE_C_COMPILER
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-g++")
set(CMAKE_AR
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-ar")
set(CMAKE_RANLIB
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-ranlib")
set(CMAKE_STRIP
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-strip")

set(CMAKE_SYSROOT
    "${SPROUT_ONION_TOOLCHAIN_ROOT}/arm-linux-gnueabihf/libc")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(SPROUT_ONION_ARCH_FLAGS
    "-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve")
set(CMAKE_C_FLAGS_INIT "${SPROUT_ONION_ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${SPROUT_ONION_ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
