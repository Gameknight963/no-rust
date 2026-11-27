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
transpile("${SOURCE_DIR}/examples/data-types.rs" "${TEST_DIR}/data-demo.cpp")
transpile("${SOURCE_DIR}/tests/data-types.rs" "${TEST_DIR}/data-checks.cpp")
transpile("${SOURCE_DIR}/tests/arrays-control.rs" "${TEST_DIR}/arrays-control.cpp")
file(WRITE "${TEST_DIR}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.10)\nproject(generated LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(demo demo.cpp)\nadd_executable(checks checks.cpp)\nadd_executable(data-demo data-demo.cpp)\nadd_executable(data-checks data-checks.cpp)\n")
file(APPEND "${TEST_DIR}/CMakeLists.txt" "add_executable(arrays-control arrays-control.cpp)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${TEST_DIR}" -B "${TEST_DIR}/build" -G "${GENERATOR}" RESULT_VARIABLE status OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Generated project configuration failed: ${log}${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_DIR}/build" --config "${CONFIG}" RESULT_VARIABLE status OUTPUT_VARIABLE log ERROR_VARIABLE error)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Generated C++ compilation failed: ${log}${error}")
endif()
foreach(target demo checks data-demo data-checks arrays-control)
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
  execute_process(COMMAND "${executable}" RESULT_VARIABLE status OUTPUT_VARIABLE actual ERROR_VARIABLE error TIMEOUT 15)
  string(REPLACE "\r\n" "\n" actual "${actual}")
  if(target STREQUAL "demo")
    set(expected "The sum of squares is 55\n3...\n2...\n1...\nYour code passed the borrow checker. We fixed that.\n")
  elseif(target STREQUAL "checks")
    set(expected "{answer} 14 120 true\nhello\n6\ngood\n\n")
  elseif(target STREQUAL "data-demo")
    set(expected "Power: 9002. Approved: true\nScore: 25\nThe compiler sensed your power.\nQuit score: 0\n")
  elseif(target STREQUAL "arrays-control")
    set(expected "3 7 6\n4 9\n0\n3\n-10\n0\n10\n11\n4\n3\n26\n5\n7\n17\n")
  else()
    set(expected "13 13\n42\n5 8\n7 90\n7\n0\nhello\n100\n12\n200\n300\n7\n11\n12\n99\n2\nloop 0\nfields work\n20\n2\n40\n21\n")
  endif()
  if(NOT status EQUAL 0 OR NOT actual STREQUAL expected)
    message(FATAL_ERROR "${target}: expected [${expected}], got [${actual}], status ${status}: ${error}")
  endif()
endforeach()
foreach(source "trait Foo {}" "fn main() { println!(\"{}\"); }" "fn main() { let x = 1 }" "/* unterminated"
    "struct Foo { x: i32 } fn main() { let foo = Foo { y: 1 }; }"
    "struct Foo { x: i32 } fn main() { let foo = Foo {}; }"
    "enum E { A(i32) } fn main() { match E::A(1) { E::A() => {} } }"
    "enum E { A } fn main() { match E::A { _ => {}, E::A => {} } }"
    "enum E { A } fn main() { match E::A { E::A if true => {} } }"
    "enum E { A } fn main() { let value = match E::A { E::A => { return; } }; }"
    "enum E { A } fn main() { match E::A { E::Missing => {} } }"
    "enum E { A { x: i32 } } fn main() { match E::A { x: 1 } { E::A { y } => {} } }"
    "fn main() { let x = if true { 1 }; }"
    "fn main() { let x = if true { return; } else { 2 }; }"
    "fn main() { let x = [1, 2; }"
    "fn main() { let x = []; }")
  file(WRITE "${TEST_DIR}/invalid.rs" "${source}")
  execute_process(COMMAND "${TRANSPILER}" "${TEST_DIR}/invalid.rs" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(status EQUAL 0 OR NOT output STREQUAL "" OR NOT error MATCHES ":[0-9]+:[0-9]+:")
    message(FATAL_ERROR "Expected a located diagnostic for [${source}], got ${status}: ${error}")
  endif()
endforeach()
