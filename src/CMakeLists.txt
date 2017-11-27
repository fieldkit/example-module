set(SOURCE_FILES
  example-module.cpp
)

set(LIBRARIES
  SPI
  Wire
)

arduino(example-module "${SOURCE_FILES}" "${LIBRARIES}")
