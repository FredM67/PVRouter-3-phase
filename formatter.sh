if ! which clang-format &>/dev/null; then
  echo "clang-format not found or not in PATH. Please install: https://github.com/arduino/clang-static-binaries/releases"
  exit 1
fi

find . -name "*.cpp" -o -name "*.ino" -o -name "*.c" -o -name "*.h"|xargs -I {} clang-format -i --style=file {}