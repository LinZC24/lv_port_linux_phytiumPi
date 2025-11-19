# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# make  -C build -j

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(tools /opt/aarch64-buildroot-linux-gnu_sdk-buildroot)
set(CMAKE_C_COMPILER ${tools}/bin/aarch64-linux-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/aarch64-linux-g++)

set(Python_EXECUTABLE /usr/bin/python3)
set(Python3_EXECUTABLE /usr/bin/python3)

set(CMAKE_SYSROOT ${tools}/aarch64-buildroot-linux-gnu/sysroot)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

# If necessary, set STAGING_DIR
# if not work, please try(in shell command): export STAGING_DIR=/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target
#set(ENV{STAGING_DIR} "/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target")

