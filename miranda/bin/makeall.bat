@echo off
echo ------------------------------------------------------------------------------------------------------------------------
echo ---------------------------------------------------- build ANSI --------------------------------------------------------
echo ------------------------------------------------------------------------------------------------------------------------
call make.bat
echo ------------------------------------------------------------------------------------------------------------------------
echo ---------------------------------------------------- build Unicode -----------------------------------------------------
echo ------------------------------------------------------------------------------------------------------------------------
call makew.bat
echo ------------------------------------------------------------------------------------------------------------------------
echo ---------------------------------------------------- build x64 ---------------------------------------------------------
echo ------------------------------------------------------------------------------------------------------------------------
call make.64bat
echo ------------------------------------------------------------------------------------------------------------------------
echo ---------------------------------------------------- build Langpack ----------------------------------------------------
echo ------------------------------------------------------------------------------------------------------------------------
call makel.bat
echo ------------------------------------------------------------------------------------------------------------------------
echo ---------------------------------------------------- all finished ------------------------------------------------------
echo ------------------------------------------------------------------------------------------------------------------------
