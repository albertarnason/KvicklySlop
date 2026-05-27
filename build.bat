@echo off

set CommonCompilerFlags=-DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -DHANDMADE_WIN32=1 -Gm- -GR- -MTd -nologo -EHa- -Od -Oi -W4 -WX -wd4505 -wd4201 -wd4100 -wd4189 -wd4459 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib

IF NOT EXIST build mkdir build
pushd build

REM Game DLL
cl %CommonCompilerFlags% /I "..\SlopEngine\engine" "..\SlopEngine\engine\handmade.cpp" -Fmhandmade.map /LD /link -incremental:no /PDB:handmade.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender

REM Win32 platform layer
cl %CommonCompilerFlags% /I "..\SlopEngine\engine" /I "..\SlopEngine\windows platform" /I "..\SlopEngine\windows platform\include" "..\SlopEngine\windows platform\win32_handmade.cpp" "..\SlopEngine\windows platform\src\glad.c" -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd