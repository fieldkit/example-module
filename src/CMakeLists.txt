set(SOURCE_FILES main.cpp)
set(LIBRARIES SPI Wire)

arduino(fk-example-module-standard "${SOURCE_FILES}" "${LIBRARIES}")
