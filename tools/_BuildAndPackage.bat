echo.
echo Running CompileDLL.bat...
call _CompileDLL.bat
if %errorlevel% neq 0 (
    echo CompileDLL.bat failed.
    exit /b 1
)

echo.
echo Running CreateModPackage.bat...
call _CreateModPackage.bat
if %errorlevel% neq 0 (
    echo CreateModPackage.bat failed.
    exit /b 1
)