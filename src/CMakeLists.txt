include(Arduino)

enable_arduino_toolchain()

find_package(FkModule)

set(sources main.cpp)
add_arduino_library(fk-example-module-standard "${sources}")

fk_configure_module(fk-example-module-standard)

target_compile_options(fk-example-module-standard PRIVATE -Wall -Werror)

add_arduino_firmware(fk-example-module-standard)
