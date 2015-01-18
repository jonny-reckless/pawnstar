REM This batch file cleans up the output files, intellisense etc.

rmdir /s /q ipch
rmdir /s /q Debug
rmdir /s /q Release
rmdir /s /q x64
rmdir /s /q ReleaseX
rmdir /s /q generate_constants\ipch
rmdir /s /q generate_constants\Debug
rmdir /s /q generate_constants\Release
rmdir /s /q generate_constants\x64
del Pawnstar.sdf
del Pawnstar.ncb
del *.user
del generate_constants\*.user
del kpk.txt