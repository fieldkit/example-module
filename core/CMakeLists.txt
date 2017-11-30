set(SOURCE_FILES
  main.cpp
  attached-devices.cpp
  wifi.cpp
  fk-core.cpp
  app-protobuf.cpp
)

set(LIBRARIES
  SPI
  Wire
  ../firmware-common/modules
)

include_directories(./)

arduino(example-core "${SOURCE_FILES}" "${LIBRARIES}")
