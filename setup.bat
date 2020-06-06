
mkdir build
cmake -G "Visual Studio 15 2017 Win64" -Thost=x64 -DCMAKE_INSTALL_PREFIX=dist -B build
cmake --build build --target install
.\dist\bin\sphere.exe
