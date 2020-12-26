project(OwletWebApp)

# config with somethign like: cmake -DCMAKE_PREFIX=~/install/wt_4.3.1/ -G Xcode ..

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

if("${CMAKE_SYSTEM}" MATCHES "Linux" )
  SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif("${CMAKE_SYSTEM}" MATCHES "Linux" )

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.50 REQUIRED COMPONENTS program_options thread filesystem )

find_library( LIBZ_LIBRARY z )

find_package( Wt REQUIRED Wt HTTP Dbo DboSqlite3 )


add_executable( OwletWebApp.wt
  src/main.cpp
  src/Alarmer.h
  src/Alarmer.cpp
  src/OwletWebApp.h
  src/OwletWebApp.cpp
  src/Database.h
  src/Database.cpp
  src/OwletChart.cpp
  src/OwletChart.h
  src/Settings.h
  src/Settings.cpp
)

target_link_libraries( OwletWebApp.wt PUBLIC
  Boost::program_options
  Boost::thread
  Boost::filesystem
  ${LIBZ_LIBRARY}
)

# For some reason I cant get the proper way to link to Wt to work on my macOS
if( APPLE )
  target_link_libraries( OwletWebApp.wt PUBLIC ${Wt_LIBRARY} ${Wt_HTTP_LIBRARY} ${Wt_DBO_LIBRARY} ${Wt_DBOSQLITE3_LIBRARY} )
  target_include_directories( OwletWebApp.wt PUBLIC ${Wt_INCLUDE_DIR} )
else( APPLE )
  target_link_libraries( OwletWebApp.wt PUBLIC Wt::Wt Wt::HTTP Wt::Dbo Wt::DboSqlite3 )
endif( APPLE )


if("${CMAKE_SYSTEM}" MATCHES "Linux" AND NOT ANDROID)
  find_library( RT_LIB rt )
  
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  find_package(OpenSSL REQUIRED)
  
  target_link_libraries( OwletWebApp.wt PUBLIC ${RT_LIB} OpenSSL::SSL OpenSSL::Crypto ${CMAKE_DL_LIBS} -static-libgcc -static-libstdc++ )
endif()


if( CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" )
  target_link_libraries( OwletWebApp.wt PUBLIC -latomic )
endif( CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" )


if(APPLE AND NOT IOS)
  target_link_libraries( OwletWebApp.wt PUBLIC
         /usr/lib/libSystem.B.dylib
         /System/Library/Frameworks/Carbon.framework/Versions/A/Carbon
         /System/Library/Frameworks/Foundation.framework/Versions/C/Foundation
  )
endif(APPLE AND NOT IOS)

set_target_properties(OwletWebApp.wt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set( OTHER_SUPPORT_FILES
     ${CMAKE_CURRENT_SOURCE_DIR}/README.md
     ${CMAKE_CURRENT_SOURCE_DIR}/config/settings.ini
     ${CMAKE_CURRENT_SOURCE_DIR}/assets/css/OwletWebApp.css
     ${CMAKE_CURRENT_SOURCE_DIR}/config/wt_config.xml
)

add_custom_target( OwletWebApp_resources SOURCES ${OTHER_SUPPORT_FILES} )


if( ${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0" )
    set_target_properties(InterSpecExe PROPERTIES
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif( ${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0" )

# We'll only define an install directory for Linux
if("${CMAKE_SYSTEM}" MATCHES "Linux" )
  # For the install, we will copy Wt's 'resources' folder into our 'assets' folder
  get_target_property(WT_INCLUDES Wt::Wt INTERFACE_INCLUDE_DIRECTORIES)

  # WT_INCLUDES could be a list, but in practice it looks to be a single directory, so we'll skip the loop
  find_path( WT_RESOURCES_DIRECTORY form.css
    ${WT_INCLUDES}/../share/Wt/resources
  )

  install( TARGETS OwletWebApp.wt RUNTIME DESTINATION "OwletWebApp")
  install( DIRECTORY config assets scripts owletpy DESTINATION "OwletWebApp" )
  install( FILES LICENSE README.md DESTINATION "OwletWebApp/" )
  install( DIRECTORY DESTINATION "OwletWebApp/data" )
  install( DIRECTORY "${WT_RESOURCES_DIRECTORY}" DESTINATION "OwletWebApp/assets/" )
endif("${CMAKE_SYSTEM}" MATCHES "Linux" )
