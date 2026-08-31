@echo off

set CommonCompilerFlags=-DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_WIN32=1 -Gm- -GR- -MTd -nologo -EHa- -Od -Oi -W4 -WX -wd4505 -wd4201 -wd4100 -wd4189 -wd4459 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

set SDL3Lib=..\SlopEngine\SDL3\lib\x64
set PATH=%PATH%;%SDL3Lib%

IF NOT EXIST build mkdir build
pushd build

REM Copy SDL3.dll next to the exe if not already there
IF NOT EXIST SDL3.dll copy "..\SlopEngine\SDL3\lib\x64\SDl3.dll" .

REM Game DLL (shared by both platform layers)
cl %CommonCompilerFlags% /I "..\SlopEngine\engine" /I "..\SlopEngine\SDL3\include" "..\SlopEngine\engine\handmade.cpp" -Fmhandmade.map /LD /link -incremental:no /PDB:handmade.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender

REM SDL3 platform layer (in progress)
cl %CommonCompilerFlags% /I "..\SlopEngine\engine" /I "..\SlopEngine\windows_platform" /I "..\SlopEngine\SDL3\include" "..\SlopEngine\windows_platform\sdl_win32_platform.cpp" -Fesdl_win32_platform.exe -Fmsdl_win32_platform.map /link %CommonLinkerFlags% /LIBPATH:"..\SlopEngine\SDL3\lib\x64" SDL3.lib

popd