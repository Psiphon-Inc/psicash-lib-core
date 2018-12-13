REM Clean previous build
rmdir /S /Q build
mkdir build
chdir build

REM Make the MSVC project
cmake -G "Visual Studio 14 2015" ..

REM Build for Debug and MinSizeRel
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
msbuild.exe -p:Configuration=Debug -p:PlatformToolset=v140_xp -p:PreferredToolArchitecture=x86 psicash.vcxproj
msbuild.exe -p:Configuration=MinSizeRel -p:PlatformToolset=v140_xp -p:PreferredToolArchitecture=x86 psicash.vcxproj
REM Resulting libs (and pdb) are in build/Debug and build/MinSizeRel

chdir ..
