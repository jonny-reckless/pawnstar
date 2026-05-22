# CMake generated Testfile for 
# Source directory: /home/jonny/work/pawnstar
# Build directory: /home/jonny/work/pawnstar/build-test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_perft "/home/jonny/work/pawnstar/build-test/test_perft")
set_tests_properties(test_perft PROPERTIES  _BACKTRACE_TRIPLES "/home/jonny/work/pawnstar/CMakeLists.txt;74;add_test;/home/jonny/work/pawnstar/CMakeLists.txt;77;add_engine_test;/home/jonny/work/pawnstar/CMakeLists.txt;0;")
add_test(test_bratko_kopec "/home/jonny/work/pawnstar/build-test/test_bratko_kopec")
set_tests_properties(test_bratko_kopec PROPERTIES  _BACKTRACE_TRIPLES "/home/jonny/work/pawnstar/CMakeLists.txt;74;add_test;/home/jonny/work/pawnstar/CMakeLists.txt;78;add_engine_test;/home/jonny/work/pawnstar/CMakeLists.txt;0;")
add_test(test_see "/home/jonny/work/pawnstar/build-test/test_see")
set_tests_properties(test_see PROPERTIES  _BACKTRACE_TRIPLES "/home/jonny/work/pawnstar/CMakeLists.txt;74;add_test;/home/jonny/work/pawnstar/CMakeLists.txt;79;add_engine_test;/home/jonny/work/pawnstar/CMakeLists.txt;0;")
subdirs("_deps/googletest-build")
