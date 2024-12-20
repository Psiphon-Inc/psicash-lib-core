REM Clean previous build
del /F /S /Q dist-windows 1>nul
rmdir /S /Q dist-windows
if "%ERRORLEVEL%" == "1" exit /B 1
mkdir dist-windows

del /F /S /Q build 1>nul
rmdir /S /Q build
if "%ERRORLEVEL%" == "1" exit /B 1
mkdir build
chdir build


REM Make the MSVC project
cmake -G "Visual Studio 17 2022" -A Win32 ..
if "%ERRORLEVEL%" == "1" exit /B 1

REM Build for Debug and MinSizeRel
REM This doesn't need to be called in the GitHub Action build
if "%GITHUB_ACTION%" == "" call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if "%ERRORLEVEL%" == "1" exit /B 1
msbuild.exe -p:Configuration=Debug -p:PlatformToolset=v140 -p:PreferredToolArchitecture=x86 -p:Platform=x86 -p:PlatformTarget=x86 psicash.vcxproj
if "%ERRORLEVEL%" == "1" exit /B 1
msbuild.exe -p:Configuration=MinSizeRel -p:PlatformToolset=v140 -p:PreferredToolArchitecture=x86 -p:Platform=x86 -p:PlatformTarget=x86 psicash.vcxproj
if "%ERRORLEVEL%" == "1" exit /B 1
REM Resulting libs (and pdb) are in build/Debug and build/MinSizeRel

chdir ..

robocopy /V build\MinSizeRel\ dist-windows\Release2015 /S
robocopy /V build\Debug dist-windows\Debug2015 /S
REM TODO: put exported include files into an "include" directory and modify build appropriately
robocopy /V . dist-windows datetime.hpp error.hpp url.hpp psicash.hpp
robocopy /V vendor\ dist-windows\vendor /S /xf httplib.h
git describe --always --long --dirty --tags > dist-windows/git.txt

del /F /S /Q build 1>nul
rmdir /S /Q build
