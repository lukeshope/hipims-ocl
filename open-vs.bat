@echo off

REM ;; MS removed this environment variable. Really helpful...
set arch=x64
set VS150COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\

echo Setting environment variables...

set BOOST_BASE_DIR="C:\Boost\boost\"
set BOOST_INCLUDE_DIR="%BOOST_BASE_DIR:"=%"
set BOOST_LIB_DIR_DEBUG="%BOOST_BASE_DIR:"=%\stage\lib\
set BOOST_LIB_DIR_RELEASE="%BOOST_BASE_DIR:"=%\stage\lib\

set GDAL_BASE_DIR="C:\GDAL\gdal\"
set GDAL_INCLUDE_CORE_DIR="%GDAL_BASE_DIR:"=%\gcore\"
set GDAL_INCLUDE_PORT_DIR="%GDAL_BASE_DIR:"=%\port\"
set GDAL_LIB_DIR_DEBUG="%GDAL_BASE_DIR:"=%\"
set GDAL_LIB_DIR_RELEASE="%GDAL_BASE_DIR:"=%\"
set GDAL_DLL_DIR_DEBUG="%GDAL_BASE_DIR:"=%\"
set GDAL_DLL_DIR_RELEASE="%GDAL_BASE_DIR:"=%\"
set GDAL_DLL_FILENAME="gdal203.dll"

echo Removing quotes from variables...

set BOOST_BASE_DIR=%BOOST_BASE_DIR:"=%
set BOOST_INCLUDE_DIR=%BOOST_INCLUDE_DIR:"=%
set BOOST_LIB_DIR_DEBUG=%BOOST_LIB_DIR_DEBUG:"=%
set BOOST_LIB_DIR_RELEASE=%BOOST_LIB_DIR_RELEASE:"=%

set GDAL_BASE_DIR=%GDAL_BASE_DIR:"=%
set GDAL_INCLUDE_CORE_DIR=%GDAL_INCLUDE_CORE_DIR:"=%
set GDAL_INCLUDE_PORT_DIR=%GDAL_INCLUDE_PORT_DIR:"=%
set GDAL_LIB_DIR_DEBUG=%GDAL_LIB_DIR_DEBUG:"=%
set GDAL_LIB_DIR_RELEASE=%GDAL_LIB_DIR_RELEASE:"=%
set GDAL_DLL_DIR_DEBUG=%GDAL_DLL_DIR_DEBUG:"=%
set GDAL_DLL_DIR_RELEASE=%GDAL_DLL_DIR_RELEASE:"=%
set GDAL_DLL_FILENAME=%GDAL_DLL_FILENAME:"=%

echo Finding Visual Studio...

if "%VS100COMNTOOLS%"=="" (echo.) else (call "%VS100COMNTOOLS%\vsvars32.bat")
if "%VS110COMNTOOLS%"=="" (echo.) else (call "%VS110COMNTOOLS%\vsvars32.bat")
if "%VS120COMNTOOLS%"=="" (echo.) else (call "%VS120COMNTOOLS%\vsvars32.bat")
if "%VS130COMNTOOLS%"=="" (echo.) else (call "%VS130COMNTOOLS%\vsvars32.bat")
if "%VS140COMNTOOLS%"=="" (echo.) else (call "%VS140COMNTOOLS%\vsvars32.bat")
if "%VS150COMNTOOLS%"=="" (echo.) else (call "%VS150COMNTOOLS%\VsDevCmd.bat")

set HiPIMSConfigFile=%1

echo Starting Visual Studio...
devenv HiPIMS-OCL.sln
