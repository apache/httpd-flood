# Microsoft Developer Studio Project File - Name="flood" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=flood - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "flood.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "flood.mak" CFG="flood - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "flood - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "flood - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "flood - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /FD /c
# ADD CPP /nologo /MD /W3 /O2 /I "$(APRPATH)\include" /I "$(APRUTILPATH)\include" /I "$(OPENSSLPATH)\inc32" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "WIN32_LEAN_AND_MEAN" /D "NO_IDEA" /D "NO_RC5" /D "NO_MDC2" /Fd"Release/flood" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib wsock32.lib ws2_32.lib apr.lib aprutil.lib /nologo /subsystem:console /map /machine:I386
# ADD LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib apr.lib aprutil.lib pcreposix.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /map /machine:I386 /libpath:"$(APRPATH)\LibR" /libpath:"$(APRUTILPATH)\LibR" /libpath:"$(OPENSSLPATH)\$(SSLBIN)" /libpath:"$(REGEXPATH)\LibR"

!ELSEIF  "$(CFG)" == "flood - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /FD /c
# ADD CPP /nologo /MDd /W3 /GX /Zi /Od /I "$(APRPATH)\include" /I "$(APRUTILPATH)\include" /I "$(OPENSSLPATH)\inc32" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "WIN32_LEAN_AND_MEAN" /D "NO_IDEA" /D "NO_RC5" /D "NO_MDC2" /Fd"Debug/flood" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib apr.lib aprutil.lib /nologo /subsystem:console /incremental:no /map /debug /machine:I386
# ADD LINK32 kernel32.lib advapi32.lib wsock32.lib ws2_32.lib apr.lib aprutil.lib pcreposix.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /incremental:no /map /debug /machine:I386 /libpath:"$(APRPATH)\LibD" /libpath:"$(APRUTILPATH)\LibD" /libpath:"$(OPENSSLPATH)\$(SSLBIN)" /libpath:"$(REGEXPATH)\LibD"

!ENDIF 

# Begin Target

# Name "flood - Win32 Release"
# Name "flood - Win32 Debug"
# Begin Group "sources"

# PROP Default_Filter "*.c"
# Begin Source File

SOURCE=.\flood.c
# End Source File
# Begin Source File

SOURCE=.\flood_config.c
# End Source File
# Begin Source File

SOURCE=.\flood_easy_reports.c
# End Source File
# Begin Source File

SOURCE=.\flood_farm.c
# End Source File
# Begin Source File

SOURCE=.\flood_farmer.c
# End Source File
# Begin Source File

SOURCE=.\flood_net.c
# End Source File
# Begin Source File

SOURCE=.\flood_net_ssl.c
# End Source File
# Begin Source File

SOURCE=.\flood_profile.c
# End Source File
# Begin Source File

SOURCE=.\flood_report_relative_times.c
# End Source File
# Begin Source File

SOURCE=.\flood_round_robin.c
# End Source File
# Begin Source File

SOURCE=.\flood_simple_reports.c
# End Source File
# Begin Source File

SOURCE=.\flood_socket_generic.c
# End Source File
# Begin Source File

SOURCE=.\flood_socket_keepalive.c
# End Source File
# End Group
# Begin Group "includes"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=.\flood_config.h
# End Source File
# Begin Source File

SOURCE=.\flood_easy_reports.h
# End Source File
# Begin Source File

SOURCE=.\flood_farm.h
# End Source File
# Begin Source File

SOURCE=.\flood_farmer.h
# End Source File
# Begin Source File

SOURCE=.\flood_net.h
# End Source File
# Begin Source File

SOURCE=.\flood_net_ssl.h
# End Source File
# Begin Source File

SOURCE=.\flood_profile.h
# End Source File
# Begin Source File

SOURCE=.\flood_report_relative_times.h
# End Source File
# Begin Source File

SOURCE=.\flood_round_robin.h
# End Source File
# Begin Source File

SOURCE=.\flood_simple_reports.h
# End Source File
# Begin Source File

SOURCE=.\flood_socket_generic.h
# End Source File
# Begin Source File

SOURCE=.\flood_socket_keepalive.h
# End Source File
# End Group
# End Target
# End Project
