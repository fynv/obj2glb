cmake_minimum_required (VERSION 3.0)
project(obj2glb)

set(SOURCES
main.cpp
crc64.cpp
)

set (INCLUDE_DIR
glm/glm
.
)

set(CMAKE_CXX_STANDARD 17)
if (WIN32) 
set (DEFINES  ${DEFINES}
-D"_CRT_SECURE_NO_DEPRECATE"  
-D"_SCL_SECURE_NO_DEPRECATE" 
-D"_CRT_SECURE_NO_WARNINGS"
)
else()
add_compile_options(-fPIC)
endif()


include_directories(${INCLUDE_DIR})
add_definitions(${DEFINES})
add_executable(obj2glb ${SOURCES})






