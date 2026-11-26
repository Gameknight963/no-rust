# CMakeList.txt : CMake project for no-rust, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (no-rust "no-rust.cpp" "no-rust.h")

set_target_properties(no-rust PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
