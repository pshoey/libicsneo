option ( LIBFTDI_PYTHON_BINDINGS "Build python bindings via swig" ON )
option ( LIBFTDI_LINK_PYTHON_LIBRARY "Link against python libraries" ON )

if ( LIBFTDI_PYTHON_BINDINGS )
  # workaround for cmake bug #0013449
  if ( NOT DEFINED CMAKE_FIND_ROOT_PATH )
    find_package ( SWIG )
  else ()
    find_program ( SWIG_EXECUTABLE NAMES swig2.0 swig )
    if ( SWIG_EXECUTABLE )
      set ( SWIG_USE_FILE ${CMAKE_ROOT}/Modules/UseSWIG.cmake )
      set ( SWIG_FOUND TRUE )
    endif ()
  endif ()
  find_package ( PythonLibs )
  find_package ( PythonInterp )
endif ()

if ( SWIG_FOUND AND PYTHONLIBS_FOUND AND PYTHONINTERP_FOUND )
  include ( UseSWIG )
  include_directories ( BEFORE ${CMAKE_SOURCE_DIR}/src )
  include_directories ( ${PYTHON_INCLUDE_DIRS} )
  link_directories ( ${CMAKE_CURRENT_BINARY_DIR}/../src )

  if ( DOCUMENTATION AND DOXYGEN_FOUND )
    set(CMAKE_SWIG_FLAGS -DDOXYGEN=${DOXYGEN_FOUND})
  endif()
  swig_add_module ( ftdi1 python ftdi1.i )
  swig_link_libraries ( ftdi1 ftdi1 )

  if ( LIBFTDI_LINK_PYTHON_LIBRARY )
    swig_link_libraries ( ftdi1 ${PYTHON_LIBRARIES} )
  elseif( APPLE )
    set_target_properties ( ${SWIG_MODULE_ftdi1_REAL_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup" )
  endif ()

  set_target_properties ( ${SWIG_MODULE_ftdi1_REAL_NAME} PROPERTIES NO_SONAME ON )

  execute_process ( COMMAND ${PYTHON_EXECUTABLE} -c "from distutils import sysconfig; print( sysconfig.get_python_lib( plat_specific=True, prefix='${CMAKE_INSTALL_PREFIX}' ) )"
                    OUTPUT_VARIABLE _ABS_PYTHON_MODULE_PATH
                    OUTPUT_STRIP_TRAILING_WHITESPACE )

  get_filename_component ( _ABS_PYTHON_MODULE_PATH ${_ABS_PYTHON_MODULE_PATH} ABSOLUTE )
  file ( RELATIVE_PATH _REL_PYTHON_MODULE_PATH ${CMAKE_INSTALL_PREFIX} ${_ABS_PYTHON_MODULE_PATH} )

  set ( PYTHON_MODULE_PATH
        ${_REL_PYTHON_MODULE_PATH}
      )

  install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/ftdi1.py DESTINATION ${PYTHON_MODULE_PATH} )
  install ( TARGETS ${SWIG_MODULE_ftdi1_REAL_NAME} LIBRARY DESTINATION ${PYTHON_MODULE_PATH} )

  if ( DOCUMENTATION AND DOXYGEN_FOUND )
      # Run doxygen to only generate the xml
      add_custom_command ( OUTPUT ${CMAKE_BINARY_DIR}/doc/xml/ftdi_8c.xml
          COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
          COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile.xml
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          DEPENDS ${c_headers};${c_sources};${cpp_sources};${cpp_headers}
      )

      # generate .i from doxygen .xml
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ftdi1_doc.i
          COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doxy2swig.py -n
              ${CMAKE_BINARY_DIR}/doc/xml/ftdi_8c.xml
              ${CMAKE_CURRENT_BINARY_DIR}/ftdi1_doc.i
          DEPENDS ${CMAKE_BINARY_DIR}/doc/xml/ftdi_8c.xml
      )
      add_custom_target ( doc_i DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ftdi1_doc.i )
      add_dependencies( ${SWIG_MODULE_ftdi1_REAL_NAME} doc_i )

  endif ()

  set ( LIBFTDI_PYTHON_MODULE_PATH ${CMAKE_INSTALL_PREFIX}/${PYTHON_MODULE_PATH} )
  set ( LIBFTDI_PYTHON_MODULE_PATH ${LIBFTDI_PYTHON_MODULE_PATH} PARENT_SCOPE ) # for ftdiconfig.cmake
  message(STATUS "Building python bindings via swig. Will be installed under ${LIBFTDI_PYTHON_MODULE_PATH}")

  add_subdirectory ( examples )
else ()
  message(STATUS "Not building python bindings")
endif ()
