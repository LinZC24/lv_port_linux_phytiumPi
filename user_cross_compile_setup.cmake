# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=./user_cross_compile_setup.cmake -B build -S .
# make  -C build -j

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(tools /opt/aarch64-buildroot-linux-gnu_sdk-buildroot)
set(CMAKE_C_COMPILER ${tools}/bin/aarch64-linux-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/aarch64-linux-g++)
set(SYSROOT ${tools}/aarch64-buildroot-linux-gnu/sysroot)

# If necessary, set STAGING_DIR
# if not work, please try(in shell command): export STAGING_DIR=/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target
#set(ENV{STAGING_DIR} "/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target")

