# Copyright (c) 2010 Daniel Pfeifer <daniel@pfeifer-mail.de>

if(NOT CMAKE_HOST_WIN32)
  return()
endif(NOT CMAKE_HOST_WIN32)

set(GNUWIN32_NAME GnuWin32)
set(GNUWIN32_TGZ ${CMAKE_SOURCE_DIR}/externals/${GNUWIN32_NAME}.tar.gz)
set(GNUWIN32_DIR ${CMAKE_BINARY_DIR}/${GNUWIN32_NAME})

if(NOT EXISTS ${GNUWIN32_DIR})
  file(MAKE_DIRECTORY ${GNUWIN32_DIR})
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf
    ${GNUWIN32_TGZ} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif(NOT EXISTS ${GNUWIN32_DIR})

file(WRITE ${CMAKE_BINARY_DIR}/unistd.h.in "\n")
configure_file(${CMAKE_BINARY_DIR}/unistd.h.in
  ${CMAKE_BINARY_DIR}/include/unistd.h COPYONLY)

set(BISON_EXECUTABLE ${GNUWIN32_DIR}/bin/bison.bat)
set(FLEX_EXECUTABLE ${GNUWIN32_DIR}/bin/flex.exe)
