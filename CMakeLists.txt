project(siadrive)
include(ExternalProject)
cmake_minimum_required(VERSION 3.3)


#Common Configuration Pre-setup
string(REPLACE "\\" "/" SIADRIVE_INSTALL_FOLDER ${SIADRIVE_INSTALL_FOLDER})
set(EXTERNAL_BUILD_ROOT ${CMAKE_BINARY_DIR}/external)
set(EXTERNAL_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(3RD_PARTY_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ttmath-0.9.3 ${EXTERNAL_BUILD_ROOT}/include)
set(COMMON_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include/siadrive_api)
add_definitions(-DTTMATH_NOASM)


#SiaDrive API Library
file(GLOB_RECURSE SIADRIVE_API_SOURCES 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_api/*.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_api/*.cxx 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_api/*.c)
  
add_library(siadrive.api SHARED ${SIADRIVE_API_SOURCES})
set_target_properties(siadrive.api
    PROPERTIES COMPILE_FLAGS -DSIADRIVE_EXPORT_SYMBOLS
)                 
  
if (MSVC)
  set(DOKANY_VERSION dokany-1.0.3)
	ExternalProject_Add(curl_project 
    URL https://github.com/curl/curl/archive/curl-7_53_1.tar.gz 
    PREFIX ${EXTERNAL_BUILD_ROOT}/builds/curl
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DCURL_STATICLIB=ON -DBUILD_TESTING=OFF -DBUILD_CURL_EXE=OFF -DCURL_DISABLE_LDAP=ON -DCMAKE_INSTALL_PREFIX=${EXTERNAL_BUILD_ROOT})
	
	ExternalProject_Add(sqlitecpp_project 
    URL https://github.com/SRombauts/SQLiteCpp/archive/2.0.0.tar.gz
    PREFIX ${EXTERNAL_BUILD_ROOT}/builds/sqlitecpp
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${EXTERNAL_BUILD_TYPE} -DSQLITECPP_RUN_CPPLINT=OFF -DSQLITECPP_RUN_CPPCHECK=OFF
    INSTALL_COMMAND cmake -E echo "Skipping install step.")
							
	add_definitions(-DCURL_STATICLIB=ON -DCURL_DISABLE_LDAP=ON)
	add_dependencies(siadrive.api curl_project sqlitecpp_project)

	target_include_directories(siadrive.api PUBLIC 
    ${EXTERNAL_BUILD_ROOT}/builds/sqlitecpp/src/sqlitecpp_project/sqlite3 
    ${EXTERNAL_BUILD_ROOT}/builds/sqlitecpp/src/sqlitecpp_project/include/)
  
	target_link_libraries(siadrive.api Shlwapi.lib Ws2_32.lib crypt32.lib winmm.lib comctl32.lib
		${EXTERNAL_BUILD_ROOT}/lib/libcurl.lib
		${EXTERNAL_BUILD_ROOT}/builds/sqlitecpp/src/sqlitecpp_project-build/sqlite3/${CMAKE_BUILD_TYPE}/sqlite3.lib
		${EXTERNAL_BUILD_ROOT}/builds/sqlitecpp/src/sqlitecpp_project-build/${CMAKE_BUILD_TYPE}/SQLiteCpp.lib)
endif()


#Sia Dokan
if (MSVC)
	file(GLOB_RECURSE SIADRIVE_DOKAN_API_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_dokan_api/*.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_dokan_api/*.cxx 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive_dokan_api/*.c)
    
	add_library(siadrive.dokan.api SHARED ${SIADRIVE_DOKAN_API_SOURCES})
	add_dependencies(siadrive.dokan.api siadrive.api)
	set_target_properties(siadrive.dokan.api
		PROPERTIES COMPILE_FLAGS -DSIADRIVE_DOKAN_EXPORT_SYMBOLS
	)                                                                                                                                         
	target_include_directories(siadrive.dokan.api PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/dokan
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/sys
    ${CMAKE_CURRENT_SOURCE_DIR}/include/siadrive_dokan_api)
	target_link_libraries(siadrive.dokan.api siadrive.api ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokan1.lib)
endif()


#SiaDrive
file(GLOB_RECURSE SIADRIVE_SOURCES 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive/*.cpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive/*.cxx 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/siadrive/*.c)
  
add_executable(siadrive WIN32 ${SIADRIVE_SOURCES}) 
add_dependencies(siadrive siadrive.api siadrive.dokan.api)
target_include_directories(siadrive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/siadrive)
target_link_libraries(siadrive PRIVATE siadrive.api siadrive.dokan.api)

if (MSVC)
  target_include_directories(siadrive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/chromium_git/cef)
  target_link_libraries(siadrive PRIVATE
  	${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/lib/${CMAKE_BUILD_TYPE}/libcef_dll_wrapper.lib
  	${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/lib/${CMAKE_BUILD_TYPE}/libcef.dll.lib
  	${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/lib/${CMAKE_BUILD_TYPE}/chrome_elf.dll.lib)
  
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set_target_properties(siadrive PROPERTIES COMPILE_FLAGS "/MTd")
  else()
      set_target_properties(siadrive PROPERTIES COMPILE_FLAGS "/MT")
  endif()      

  set(CEF_LIBS 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libcef.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/chrome_elf.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/osmesa.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libEGL.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libGLESv2.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/d3dcompiler_47.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokan1.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokanfuse1.dll
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokannp1.dll)
    
  file(GLOB CEF_SUPPORT 
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/icudtl.dat
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/cef.pak
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/*.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/cef_*.pak)

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CEF_LIBS ${CEF_LIBS}
      ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/siadrive.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libcef.dll.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/chrome_elf.dll.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/osmesa.dll.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libEGL.dll.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libGLESv2.dll.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/libcef_dll_wrapper_cc.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokan1.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokanfuse1.pdb
      ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/${DOKANY_VERSION}/x64/${CMAKE_BUILD_TYPE}/dokannp1.pdb)
  endif()

  SET ( CMAKE_EXE_LINKER_FLAGS /MANIFEST:NO )
  add_custom_command(
    TARGET siadrive
    POST_BUILD
    COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\src\\siadrive\\siadrive.exe.manifest\" -outputresource:"$(TargetDir)$(TargetFileName)"
    COMMENT "Adding manifest..."
  )
endif()       


# Windows installation        
if (MSVC)                    
  install(FILES ${CEF_LIBS} DESTINATION ${SIADRIVE_INSTALL_FOLDER})
  install(FILES ${CEF_SUPPORT} DESTINATION ${SIADRIVE_INSTALL_FOLDER})
  install(TARGETS siadrive.api siadrive.dokan.api DESTINATION ${SIADRIVE_INSTALL_FOLDER})
  install(TARGETS siadrive DESTINATION ${SIADRIVE_INSTALL_FOLDER})
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/CEF/bin/${CMAKE_BUILD_TYPE}/locales DESTINATION ${SIADRIVE_INSTALL_FOLDER})
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/htdocs DESTINATION ${SIADRIVE_INSTALL_FOLDER})
endif()

        
#Common Configuration Post-setup
include_directories(${3RD_PARTY_INCLUDES} ${COMMON_INCLUDES})