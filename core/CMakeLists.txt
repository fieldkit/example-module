set(SOURCE_FILES
  example-core.cpp
  attached-devices.cpp
  wifi.cpp
  fk-core.cpp
  app-protobuf.cpp
)

set(LIBRARIES
  SPI
  Wire
  ../modules
)

include_directories(./)

arduino(example-core "${SOURCE_FILES}" "${LIBRARIES}")
