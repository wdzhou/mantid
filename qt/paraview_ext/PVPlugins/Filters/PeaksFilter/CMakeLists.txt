PROJECT(PeaksFilter)
include_directories( SYSTEM ${PARAVIEW_INCLUDE_DIRS} )
ADD_PARAVIEW_PLUGIN(MantidParaViewPeaksFilterSMPlugin "1.0"
SERVER_MANAGER_XML PeaksFilter.xml
SERVER_MANAGER_SOURCES vtkPeaksFilter.cxx vtkPeaksFilter.h)
# Add to the 'VatesParaViewPlugins' group in VS
set_property( TARGET MantidParaViewPeaksFilterSMPlugin PROPERTY FOLDER "MantidVatesParaViewPlugins")

target_link_libraries( MantidParaViewPeaksFilterSMPlugin LINK_PRIVATE ${TCMALLOC_LIBRARIES_LINKTIME} 
${CORE_MANTIDLIBS}
DataObjects
VatesAPI
${POCO_LIBRARIES}
${Boost_LIBRARIES}
${vtkjsoncpp_LIBRARIES}
)

if (OSX_VERSION VERSION_GREATER 10.8)
  set_target_properties ( MantidParaViewPeaksFilterSMPlugin PROPERTIES INSTALL_RPATH "@loader_path/../../Contents/Libraries;@loader_path/../../Contents/MacOS")
endif ()

# Put library into subfolder.
SET_TARGET_OUTPUT_DIRECTORY(MantidParaViewPeaksFilterSMPlugin  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/${PVPLUGINS_DIR}/${PVPLUGINS_SUBDIR})

install( TARGETS MantidParaViewPeaksFilterSMPlugin ${SYSTEM_PACKAGE_TARGET} DESTINATION ${PVPLUGINS_DIR}/${PVPLUGINS_SUBDIR})
