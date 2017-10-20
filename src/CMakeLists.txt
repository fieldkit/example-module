set(SOURCE_FILES
  fk-module-dummy.cpp
  fk-pool.cpp
  fk-module.cpp
  fk-core.cpp
  attached-devices.cpp
  debug.cpp
  readings.cpp
  protobuf.cpp
  app-protobuf.cpp
  comms.cpp
)

set(LIBRARIES
  SPI
  Wire
)

include_directories(./)

arduino(fk-module-dummy "${SOURCE_FILES}" "${LIBRARIES}")
