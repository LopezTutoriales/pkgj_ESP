add_executable(pkgj_cli
  src/db.cpp
  src/download.cpp
  src/simulator.cpp
  src/aes128.c
  src/sha256.c
  src/filehttp.cpp
  src/zrif.c
  src/puff.c
  src/cli.cpp
)

target_link_libraries(pkgj_cli
  CONAN_PKG::fmt
  CONAN_PKG::boost_scope_exit
  CONAN_PKG::boost_algorithm
  CONAN_PKG::sqlite3
)
