@echo off
cd /d "%~dp0"
mkdir release\lobby_connect
del /Q release\lobby_connect\*
call build_set_protobuf_directories.bat
"%PROTOC_X64_EXE%" -I.\dll\ --cpp_out=.\dll\ .\dll\net.proto
call build_env_x64.bat
cl dll/rtlgenrandom.c dll/rtlgenrandom.def
cl /DNO_DISK_WRITES /DLOBBY_CONNECT /DEMU_RELEASE_BUILD /DNDEBUG /I%PROTOBUF_X64_DIRECTORY%\include\ lobby_connect_dll.cpp dll/*.cpp dll/*.cc "%PROTOBUF_X64_LIBRARY%" Iphlpapi.lib Ws2_32.lib rtlgenrandom.lib Shell32.lib Comdlg32.lib /EHsc /MP12 /Ox /link /DLL /debug:none /OUT:release\lobby_connect\lobby_connect_x64.dll
del /Q /S release\lobby_connect\*.lib
del /Q /S release\lobby_connect\*.exp
xcopy /Y "release\lobby_connect\lobby_connect_x64.dll" "..\..\dist\lobby_connect_x64.dll"
