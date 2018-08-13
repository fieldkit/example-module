set(SOURCE_FILES main.cpp)
set(LIBRARIES SPI Wire)

arduino(fk-example-module-standard "${SOURCE_FILES}" "${LIBRARIES}")

add_definitions(-DPHYLUM_ENABLE_SERIAL_FLASH -DPHYLUM_ENABLE_SD)
target_compile_options(arduino_zero_phylum PUBLIC -DPHYLUM_ENABLE_SERIAL_FLASH -DPHYLUM_ENABLE_SD)
