@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set PATH=%PATH%;C:\Program Files\CMake\bin
cd /d "C:\BlackLysStudio\blacklys-cpp\build"
cmake .. -G "NMake Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2019_64 -DCMAKE_BUILD_TYPE=Release
nmake
