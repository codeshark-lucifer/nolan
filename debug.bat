@echo off
set OUTPUT=note.txt

if exist "%OUTPUT%" del "%OUTPUT%"

for /f "delims=" %%F in ('dir /s /b engine\*.h engine\*.hpp engine\*.cpp app\*.h app\*.hpp app\*.cpp assets\*.vert assets\*.frag') do (
    >>"%OUTPUT%" echo ============================================================
    >>"%OUTPUT%" echo FILE: %%F
    >>"%OUTPUT%" echo ============================================================
    type "%%F">>"%OUTPUT%"
    >>"%OUTPUT%" echo.
    >>"%OUTPUT%" echo.
)

echo Done.
pause