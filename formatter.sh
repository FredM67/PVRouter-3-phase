if ! which clang-format &>/dev/null; then
  echo "clang-format not found or not in PATH. Please install: https://github.com/arduino/clang-static-binaries/releases"
  exit 1
fi

# Check if exactly one argument is provided
if [ $# -ne 1 ]; then
  echo "Usage: $0 <folder>"
  exit 1
fi

# Check if the argument is a directory
if [ ! -d "$1" ]; then
  echo "Error: '$1' is not a directory"
  exit 1
fi

find "$1" -name "*.cpp" -o -name "*.ino" -o -name "*.c" -o -name "*.h"|xargs -I {} clang-format -i --style=file {}
