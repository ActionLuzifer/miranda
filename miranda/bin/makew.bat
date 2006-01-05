@echo off

for /F "tokens=1,2 delims= " %%i in (build.no) do call :WriteVer %%i %%j

rem ---------------------------------------------------------------------------
rem Main modules
rem ---------------------------------------------------------------------------

cd ..\src
nmake /f miranda32.mak CFG="miranda32 - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\..\miranda-tools\dbtool
nmake /f dbtool.mak CFG="dbtool - Win32 Release"
if errorlevel 1 goto :Error

rem ---------------------------------------------------------------------------
rem Protocols
rem ---------------------------------------------------------------------------

cd ..\..\miranda\protocols\IcqOscarJ
nmake /f IcqOscar8.mak CFG="icqoscar8 - Win32 Release"
if errorlevel 1 goto :Error

cd ..\MSN
nmake /f MSN.mak CFG="msn - Win32 Release"
if errorlevel 1 goto :Error

cd ..\JabberG
nmake /f jabber.mak CFG="jabberg - Win32 Release"
if errorlevel 1 goto :Error

cd ..\AimTOC2
nmake /f aim.mak CFG="aim - Win32 Release"
if errorlevel 1 goto :Error

cd ..\YAHOO
nmake /f Yahoo.mak CFG="Yahoo - Win32 Release"
if errorlevel 1 goto :Error

cd ..\IRC
nmake /f IRC.mak CFG="IRC - Win32 Release"
if errorlevel 1 goto :Error

rem ---------------------------------------------------------------------------
rem Plugins
rem ---------------------------------------------------------------------------

cd ..\..\plugins\chat
nmake /f chat.mak CFG="chat - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\clist
nmake /f clist.mak CFG="clist - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\clist_nicer
nmake /f clist.mak CFG="clist_nicer - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\help
nmake /f help.mak CFG="help - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\mwclist
nmake /f mwclist.mak CFG="mwclist - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\srmm
nmake /f srmm.mak CFG="srmm - Win32 Release Unicode"
if errorlevel 1 goto :Error

cd ..\tabSRMM
nmake /f tabSRMM.mak CFG="tabSRMM - Win32 Release Unicode"
if errorlevel 1 goto :Error

rem ---------------------------------------------------------------------------
rem Zip it
rem ---------------------------------------------------------------------------

cd "..\..\bin\Release Unicode"
copy ..\release\dbtool.exe
copy ..\release\Plugins\AIM.dll      Plugins
copy ..\release\Plugins\dbx_3x.dll   Plugins
copy ..\release\Plugins\ICQ.dll      Plugins
copy ..\release\Plugins\import.dll   Plugins
copy ..\release\Plugins\IRC.dll      Plugins
copy ..\release\Plugins\jabber.dll   Plugins
copy ..\release\Plugins\msn.dll      Plugins
copy ..\release\Plugins\png2dib.dll  Plugins
copy ..\release\Plugins\Yahoo.dll    Plugins

for /F "tokens=1,2 delims= " %%i in (..\build.no) do call :Pack %%i %%j
exit

:WriteVer
echo #ifndef _MAC >..\src\version.rc
echo ///////////////////////////////////////////////////////////////////////////// >>..\src\version.rc
echo //                                                                            >>..\src\version.rc
echo // Version                                                                    >>..\src\version.rc
echo //                                                                            >>..\src\version.rc
echo.                                                                              >>..\src\version.rc
echo VS_VERSION_INFO VERSIONINFO                                                   >>..\src\version.rc
echo  FILEVERSION 0,4,3,%2                                                         >>..\src\version.rc
echo  PRODUCTVERSION 0,4,3,%2                                                      >>..\src\version.rc
echo  FILEFLAGSMASK 0x3fL                                                          >>..\src\version.rc
echo #ifdef _DEBUG                                                                 >>..\src\version.rc
echo  FILEFLAGS 0x1L                                                               >>..\src\version.rc
echo #else                                                                         >>..\src\version.rc
echo  FILEFLAGS 0x0L                                                               >>..\src\version.rc
echo #endif                                                                        >>..\src\version.rc
echo  FILEOS 0x40004L                                                              >>..\src\version.rc
echo  FILETYPE 0x1L                                                                >>..\src\version.rc
echo  FILESUBTYPE 0x0L                                                             >>..\src\version.rc
echo BEGIN                                                                         >>..\src\version.rc
echo     BLOCK "StringFileInfo"                                                    >>..\src\version.rc
echo     BEGIN                                                                     >>..\src\version.rc
echo         BLOCK "000004b0"                                                      >>..\src\version.rc
echo         BEGIN                                                                 >>..\src\version.rc
echo             VALUE "Comments", "Licensed under the terms of the GNU General Public License\0" >>..\src\version.rc
echo             VALUE "CompanyName", " \0"                                        >>..\src\version.rc
echo             VALUE "FileDescription", "Miranda IM\0"                           >>..\src\version.rc
echo             VALUE "FileVersion", "0.4.3 alpha build #%2\0"                    >>..\src\version.rc
echo             VALUE "InternalName", "miranda32\0"                               >>..\src\version.rc
echo             VALUE "LegalCopyright", "Copyright � 2000-2005 Richard Hughes, Roland Rabien, Tristan Van de Vreede, Martin �berg, Robert Rainwater, Sam Kothari and Lyon Lim\0" >>..\src\version.rc
echo             VALUE "LegalTrademarks", "\0"                                     >>..\src\version.rc
echo             VALUE "OriginalFilename", "miranda32.exe\0"                       >>..\src\version.rc
echo             VALUE "PrivateBuild", "\0"                                        >>..\src\version.rc
echo             VALUE "ProductName", "Miranda IM\0"                               >>..\src\version.rc
echo             VALUE "ProductVersion", "0.4.3 alpha build #%2\0"                 >>..\src\version.rc
echo             VALUE "SpecialBuild", "\0"                                        >>..\src\version.rc
echo         END                                                                   >>..\src\version.rc
echo     END                                                                       >>..\src\version.rc
echo     BLOCK "VarFileInfo"                                                       >>..\src\version.rc
echo     BEGIN                                                                     >>..\src\version.rc
echo         VALUE "Translation", 0x0, 1200                                        >>..\src\version.rc
echo     END                                                                       >>..\src\version.rc
echo END                                                                           >>..\src\version.rc
echo.                                                                              >>..\src\version.rc
echo #endif    // !_MAC                                                            >>..\src\version.rc
echo.                                                                              >>..\src\version.rc

for /F "delims=-/. tokens=1,2,3" %%i in ('date /T') do call :SetBuildDate %%i %%j %%k
for /F "delims=:/. tokens=1,2" %%i in ('time /T') do call :SetBuildTime %%i %%j

echo ^<?xml version="1.0" ?^>                                                      >%temp%\index.xml
echo ^<rss version="2.0"^>                                                         >>%temp%\index.xml
echo      ^<channel^>                                                              >>%temp%\index.xml
echo           ^<title^>Miranda IM Alpha Builds^</title^>                          >>%temp%\index.xml
echo           ^<link^>http://files.miranda-im.org/builds/^</link^>                >>%temp%\index.xml
echo           ^<language^>en-us^</language^>                                      >>%temp%\index.xml
echo           ^<lastBuildDate^>%yy%-%mm%-%dd% %hh%:%mn%^</lastBuildDate^>         >>%temp%\index.xml
echo           ^<item^>                                                            >>%temp%\index.xml
echo                ^<title^>Miranda 0.4.3.0 alpha %2^</title^>                    >>%temp%\index.xml
echo 			   ^<link^>http://files.miranda-im.org/builds/?%yy%%mm%%dd%%hh%%mn%^</link^> >>%temp%\index.xml
echo                ^<description^>                                                >>%temp%\index.xml
echo                     Miranda 0.4.3.0 alpha %2 is now available at http://files.miranda-im.org/builds/miranda-v%1a%2.zip >>%temp%\index.xml
echo                ^</description^>                                               >>%temp%\index.xml
echo                ^<pubDate^>%yy%-%mm%-%dd% %hh%:%mn%^</pubDate^>                 >>%temp%\index.xml
echo                ^<category^>Nightly Builds</category^>                         >>%temp%\index.xml
echo                ^<author^>Miranda IM Development Team^</author^>               >>%temp%\index.xml
echo           ^</item^>                                                           >>%temp%\index.xml
echo      ^</channel^>                                                             >>%temp%\index.xml
echo ^</rss^>                                                                      >>%temp%\index.xml
goto :eof

:SetBuildDate
set dd=%1
set mm=%2
set yy=%3
goto :eof

:SetBuildTime
set hh=%1
set mn=%2
goto :eof

:Pack
del %Temp%\miranda-v%1a%2w.zip
7za.exe a -tzip -r -mx=9 %Temp%\miranda-v%1a%2w.zip ./*  ..\ChangeLog.txt

rd /Q /S %Temp%\pdbw
md %Temp%\pdbw
copy ..\..\src\Release\miranda32.pdb                           %Temp%\pdbw
copy ..\..\..\miranda-tools\dbtool\Release\dbtool.pdb          %Temp%\pdbw
rem  Protocols
copy ..\..\protocols\AimTOC2\Release\AIM.pdb                   %Temp%\pdbw
copy ..\..\protocols\IcqOscarJ\Release\ICQ.pdb                 %Temp%\pdbw
copy ..\..\protocols\IRC\Release\IRC.pdb                       %Temp%\pdbw
copy ..\..\protocols\JabberG\Release\jabber.pdb                %Temp%\pdbw
copy ..\..\protocols\MSN\Release\MSN.pdb                       %Temp%\pdbw
copy ..\..\protocols\Yahoo\Release\Yahoo.pdb                   %Temp%\pdbw
rem  Unicode plugins
copy ..\..\plugins\chat\Release_Unicode\chat.pdb               %Temp%\pdbw
copy ..\..\plugins\clist\Release_Unicode\clist_classic.pdb     %Temp%\pdbw
copy ..\..\plugins\clist_nicer\Release_Unicode\clist_nicer.pdb %Temp%\pdbw
copy ..\..\plugins\help\Release_Unicode\help.pdb               %Temp%\pdbw
copy ..\..\plugins\mwclist\Release_Unicode\clist_mw.pdb        %Temp%\pdbw
copy ..\..\plugins\srmm\Release_Unicode\srmm.pdb               %Temp%\pdbw
copy ..\..\plugins\tabSRMM\Release_Unicode\tabSRMM.pdb         %Temp%\pdbw
rem  Non-Unicode plugins
copy ..\..\plugins\db3x\Release\dbx_3x.pdb                     %Temp%\pdbw
copy ..\..\plugins\import\Release\import.pdb                   %Temp%\pdbw
copy ..\..\plugins\png2dib\Release\png2dib.pdb                 %Temp%\pdbw
rem Zip now
7za.exe a -tzip -r -mx=9 %Temp%\miranda-pdb-v%1a%2w.zip %Temp%\pdbw/*
rd /Q /S %Temp%\pdbw
goto :eof

:Error
echo Make failed
exit
