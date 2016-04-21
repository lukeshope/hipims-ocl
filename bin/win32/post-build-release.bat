echo Copying GDAL DLL...
copy /y "%GDAL_DLL_DIR_RELEASE%\%GDAL_DLL_FILENAME%" %1

echo Building binary ZIP file...
del /F /Q dist\hipims-win32-master.zip
tools\zip\zip -j dist\hipims-win32-master.zip bin\win32\release\hipims.exe bin\win32\release\%GDAL_DLL_FILENAME%

echo Building tests ZIP file...
del /F /Q dist\hipims-test-master.zip
tools\zip\zip -r dist\hipims-test-master.zip test\* -x test\*\output\*.img
