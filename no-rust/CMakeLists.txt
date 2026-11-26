# CMakeList.txt : CMake project for no-rust, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (no-rust "no-rust.cpp" "no-rust.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET no-rust PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
