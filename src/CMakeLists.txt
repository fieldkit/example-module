set(SOURCE_FILES
  main.cpp
)

set(LIBRARIES
  SPI
  Wire
)

arduino(example-module "${SOURCE_FILES}" "${LIBRARIES}")
