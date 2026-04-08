git clone https://code.qt.io/qt/qt5.git qt6
cd qt6

git switch %1
mkdir build
CALL .\configure.bat -schannel -no-openssl -release -static -platform win32-msvc -prefix ./build -static-runtime -submodules qtbase,qtimageformats,qtsvg,qttools,qttranslations -skip tests -skip examples -gui -widgets -init-submodules
echo on branch %1
echo config complete, building...
cmake --build . --parallel
echo build done, installing...
cmake --install .
echo installed Qt %1 in static mode

cd ..
