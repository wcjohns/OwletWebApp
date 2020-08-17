project(OwletWebApp)

# config with somethign like: cmake -DBOOST_ROOT=~/install/wt_4.3.1/ -DWt_INCLUDE_DIR=~/install/wt_4.3.1/include -G Xcode ..

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

if("${CMAKE_SYSTEM}" MATCHES "Linux" )
  SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif("${CMAKE_SYSTEM}" MATCHES "Linux" )

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.50 REQUIRED COMPONENTS program_options thread filesystem )


add_executable( OwletWebApp.wt main.cpp Alarmer.h Alarmer.cpp OwletWebApp.h OwletWebApp.cpp
                Database.h Database.cpp OwletChart.cpp OwletChart.h Settings.h Settings.cpp )

find_library( LIBZ_LIBRARY z )
include( cmake/FindWt.cmake )


target_link_libraries( OwletWebApp.wt PUBLIC ${Wt_LIBRARIES} Boost::program_options Boost::thread Boost::filesystem ${LIBZ_LIBRARY} )
target_include_directories( OwletWebApp.wt PUBLIC ${Wt_INCLUDE_DIR} )

if("${CMAKE_SYSTEM}" MATCHES "Linux" AND NOT ANDROID)
  find_library( RT_LIB rt )
  
  set(OPENSSL_USE_STATIC_LIBS TRUE)
  find_package(OpenSSL REQUIRED)
  
  target_link_libraries( OwletWebApp.wt PUBLIC ${RT_LIB} OpenSSL::SSL OpenSSL::Crypto ${CMAKE_DL_LIBS} -static-libgcc -static-libstdc++ )
endif()

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
