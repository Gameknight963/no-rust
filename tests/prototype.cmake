file(MAKE_DIRECTORY "${TEST_DIR}")
function(transpile input output)
  execute_process(COMMAND "${TRANSPILER}" "${input}" -o "${output}" RESULT_VARIABLE status ERROR_VARIABLE error)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Transpilation failed: ${error}")
  endif()
endfunction()
transpile("${SOURCE_DIR}/examples/demo.rs" "${TEST_DIR}/demo.cpp")
file(WRITE "${TEST_DIR}/checks.rs" [=[
/* outer /* nested */ comment */
fn main() {
    let class = 2 + 3 * 4;
    println!("{{answer}} {} {} {}", class, factorial(5,), !false && true);
    let greeting = "hello";
    println!("{}", greeting);
    let mut sum = 0;
    for i in 0..4 { sum += i; }
    for i in 5..2 { sum += 100; }
    for i in 5..=2 { sum += 100; }
    println!("{}", sum);
    if sum == 0 { println!("bad"); }
    else if sum == 6 { println!("good"); }
    else { println!("bad"); }
    println!();
}
fn factorial(n: i32) -> i32 {
    if n <= 1 { return 1; }
    n * factorial(n - 1)
}
]=])
transpile("${TEST_DIR}/checks.rs" "${TEST_DIR}/checks.cpp")
file(WRITE "${TEST_DIR}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.10)\nproject(generated LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(demo demo.cpp)\nadd_executable(checks checks.cpp)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${TEST_DIR}" -B "${TEST_DIR}/build" -G "${GENERATOR}" RESULT_VARIABLE status OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Generated project configuration failed: ${log}${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_DIR}/build" --config "${CONFIG}" RESULT_VARIABLE status OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Generated C++ compilation failed: ${log}${error}")
endif()
foreach(target demo checks)
  set(executable "${TEST_DIR}/build/${target}${CMAKE_EXECUTABLE_SUFFIX}")
  # Script mode does not inherit the host platform's executable suffix.
  if(WIN32)
    set(executable "${TEST_DIR}/build/${target}.exe")
  endif()
  if(NOT EXISTS "${executable}")
    if(WIN32)
      set(executable "${TEST_DIR}/build/${CONFIG}/${target}.exe")
    else()
      set(executable "${TEST_DIR}/build/${CONFIG}/${target}")
    endif()
  endif()
  execute_process(COMMAND "${executable}" RESULT_VARIABLE status OUTPUT_VARIABLE actual ERROR_VARIABLE error)
  string(REPLACE "\r\n" "\n" actual "${actual}")
  if(target STREQUAL "demo")
    set(expected "The sum of squares is 55\n3...\n2...\n1...\nYour code passed the borrow checker. We fixed that.\n")
  else()
    set(expected "{answer} 14 120 true\nhello\n6\ngood\n\n")
  endif()
  if(NOT status EQUAL 0 OR NOT actual STREQUAL expected)
    message(FATAL_ERROR "${target}: expected [${expected}], got [${actual}], status ${status}: ${error}")
  endif()
endforeach()
foreach(source "struct Foo {}" "fn main() { println!(\"{}\"); }" "fn main() { let x = 1 }" "/* unterminated")
  file(WRITE "${TEST_DIR}/invalid.rs" "${source}")
  execute_process(COMMAND "${TRANSPILER}" "${TEST_DIR}/invalid.rs" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(status EQUAL 0 OR NOT output STREQUAL "" OR NOT error MATCHES ":[0-9]+:[0-9]+:")
    message(FATAL_ERROR "Expected a located diagnostic for [${source}], got ${status}: ${error}")
  endif()
endforeach()
