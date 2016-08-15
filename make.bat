@REM This is a batch file to use "make" from the Windows command line. It will
@REM search for "mingw32-make.exe" using %PATH% first, then try to use
@REM %MINGW_HOME%, %MINGWDIR% and "C:\MinGW" to locate the MinGW directory.

@SET _MINGW_MAKE=mingw32-make.exe
@WHERE /Q %_MINGW_MAKE%
@IF %ERRORLEVEL% EQU 0 GOTO GO

@SET _MINGW_MAKE=%MINGW_HOME%\bin\%_MINGW_MAKE%
@IF EXIST %_MINGW_MAKE% GOTO GO

@SET _MINGW_MAKE=%MINGWDIR%\bin\%_MINGW_MAKE%
@IF EXIST %_MINGW_MAKE% GOTO GO

@SET _MINGW_MAKE=C:\MinGW\bin\%_MINGW_MAKE%
@IF EXIST %_MINGW_MAKE% GOTO GO

@REM You may add other custom path substitutions here.

@ECHO Couldn't locate mingw32-make.exe. Make sure one of the following
@ECHO conditions is true so that the executable is found:
@ECHO   - the MinGW path is in %%PATH%%;
@ECHO   - the MinGW path is in %%MINGW_HOME%%;
@ECHO   - the MinGW path is in %%MINGWDIR%%;
@ECHO   - the MinGW path is C:\MinGW.
@GOTO END

:GO

@REM The following properly calls the "make" application with the arguments
@REM provided. Otherwise, parameters containing the equal sign ("=") would get
@REM splitted.
@FOR /F "tokens=1*" %%x IN ("skip %*") DO @%_MINGW_MAKE% %%y

:END

@EXIT /B %ERRORLEVEL%