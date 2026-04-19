#!/bin/bash
set -e

project_root="$(cd "$(dirname "$0")" && pwd -P)"

flags="-Wno-deprecated-declarations -Wno-int-to-void-pointer-cast -Wno-writable-strings -Wno-dangling-else -Wno-switch -Wno-undefined-internal -Wno-logical-op-parentheses -Wno-nullability-completeness"
exe="main"
libs="-framework Cocoa -lcurl"

pushd $project_root

mkdir -p build
pushd build

    ~/bin/ntime clang -DDEBUG=1 -I ../src $flags $libs ../src/macos_main.m -o $exe
    ./$exe
    
popd

popd
