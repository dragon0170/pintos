#!/bin/bash

if [ -z "$1" ]; then
  echo "You should specify the directory for the project. (e.g. userprog)"
  return 1
fi

dirname=${PWD}
echo $dirname
sed -i "257 c\        my \$name = find_file ('$dirname/src/$1/build/kernel.bin');" ./src/utils/pintos
sed -i "362 c\    \$name = find_file ('$dirname/src/$1/build/loader.bin') if !defined \$name;" ./src/utils/Pintos.pm
echo "export PATH=$dirname/src/utils:\$PATH" >> ~/.bashrc
source ~/.bashrc
