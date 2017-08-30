set(SOURCE_FILES
  fk-module-dummy.cpp
  fk-pool.cpp
  fk-module.cpp
  debug.cpp
)

set(LIBRARIES
  SPI
  Wire
)

include_directories(./)

arduino(fk-module-dummy "${SOURCE_FILES}" "${LIBRARIES}")
