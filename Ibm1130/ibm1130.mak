# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "ibm1130.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "Win32 Debug"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WinRel"
# PROP Intermediate_Dir "WinRel"
OUTDIR=.\WinRel
INTDIR=.\WinRel

ALL : $(OUTDIR)/ibm1130.exe $(OUTDIR)/ibm1130.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /YX /O2 /I "c:\pdp11\supnik" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "GUI_SUPPORT" /U "VMS" /FR /c
CPP_PROJ=/nologo /W3 /GX /YX /O2 /I "c:\pdp11\supnik" /D "NDEBUG" /D "WIN32" /D\
 "_CONSOLE" /D "GUI_SUPPORT" /U "VMS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"ibm1130.pch"\
 /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinRel/
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"ibm1130.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"ibm1130.bsc" 
BSC32_SBRS= \
	$(INTDIR)/ibm1130_cpu.sbr \
	$(INTDIR)/ibm1130_sys.sbr \
	$(INTDIR)/ibm1130_cr.sbr \
	$(INTDIR)/ibm1130_stddev.sbr \
	$(INTDIR)/ibm1130_disk.sbr \
	$(INTDIR)/scp_tty.sbr \
	$(INTDIR)/scp.sbr

$(OUTDIR)/ibm1130.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib\
 /NOLOGO /SUBSYSTEM:console /INCREMENTAL:no /PDB:$(OUTDIR)/"ibm1130.pdb"\
 /MACHINE:I386 /OUT:$(OUTDIR)/"ibm1130.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/ibm1130_cpu.obj \
	$(INTDIR)/ibm1130_sys.obj \
	$(INTDIR)/ibm1130_cr.obj \
	$(INTDIR)/ibm1130_stddev.obj \
	$(INTDIR)/ibm1130.res \
	$(INTDIR)/ibm1130_disk.obj \
	$(INTDIR)/scp_tty.obj \
	$(INTDIR)/scp.obj

$(OUTDIR)/ibm1130.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WinDebug"
# PROP Intermediate_Dir "WinDebug"
OUTDIR=.\WinDebug
INTDIR=.\WinDebug

ALL : $(OUTDIR)/ibm1130.exe $(OUTDIR)/ibm1130.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /W3 /GX /Zi /YX /Od /I "c:\pdp11\supnik" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "GUI_SUPPORT" /U "VMS" /FR /c
CPP_PROJ=/nologo /W3 /GX /Zi /YX /Od /I "c:\pdp11\supnik" /D "_DEBUG" /D\
 "WIN32" /D "_CONSOLE" /D "GUI_SUPPORT" /U "VMS" /FR$(INTDIR)/\
 /Fp$(OUTDIR)/"ibm1130.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"ibm1130.pdb" /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"ibm1130.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"ibm1130.bsc" 
BSC32_SBRS= \
	$(INTDIR)/ibm1130_cpu.sbr \
	$(INTDIR)/ibm1130_sys.sbr \
	$(INTDIR)/ibm1130_cr.sbr \
	$(INTDIR)/ibm1130_stddev.sbr \
	$(INTDIR)/ibm1130_disk.sbr \
	$(INTDIR)/scp_tty.sbr \
	$(INTDIR)/scp.sbr

$(OUTDIR)/ibm1130.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib\
 /NOLOGO /SUBSYSTEM:console /INCREMENTAL:yes /PDB:$(OUTDIR)/"ibm1130.pdb" /DEBUG\
 /MACHINE:I386 /OUT:$(OUTDIR)/"ibm1130.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/ibm1130_cpu.obj \
	$(INTDIR)/ibm1130_sys.obj \
	$(INTDIR)/ibm1130_cr.obj \
	$(INTDIR)/ibm1130_stddev.obj \
	$(INTDIR)/ibm1130.res \
	$(INTDIR)/ibm1130_disk.obj \
	$(INTDIR)/scp_tty.obj \
	$(INTDIR)/scp.obj

$(OUTDIR)/ibm1130.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "Source Files"

################################################################################
# Begin Source File

SOURCE=.\ibm1130_cpu.c
DEP_IBM11=\
	.\ibm1130_defs.h\
	..\sim_defs.h

$(INTDIR)/ibm1130_cpu.obj :  $(SOURCE)  $(DEP_IBM11) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ibm1130_sys.c
DEP_IBM113=\
	.\ibm1130_defs.h\
	..\sim_defs.h

$(INTDIR)/ibm1130_sys.obj :  $(SOURCE)  $(DEP_IBM113) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ibm1130_cr.c
DEP_IBM1130=\
	.\ibm1130_defs.h\
	..\sim_defs.h

$(INTDIR)/ibm1130_cr.obj :  $(SOURCE)  $(DEP_IBM1130) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ibm1130_stddev.c
DEP_IBM1130_=\
	.\ibm1130_defs.h\
	.\ibm1130_conout.h\
	.\ibm1130_conin.h\
	.\ibm1130_prtwheel.h\
	..\sim_defs.h

$(INTDIR)/ibm1130_stddev.obj :  $(SOURCE)  $(DEP_IBM1130_) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ibm1130.rc
DEP_IBM1130_R=\
	.\1130consoleblank.bmp\
	.\HAND.CUR

$(INTDIR)/ibm1130.res :  $(SOURCE)  $(DEP_IBM1130_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ibm1130_disk.c
DEP_IBM1130_D=\
	.\ibm1130_defs.h\
	..\sim_defs.h

$(INTDIR)/ibm1130_disk.obj :  $(SOURCE)  $(DEP_IBM1130_D) $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=..\scp_tty.c
DEP_SCP_T=\
	..\sim_defs.h

$(INTDIR)/scp_tty.obj :  $(SOURCE)  $(DEP_SCP_T) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\scp.c
DEP_SCP_C=\
	.\sim_defs.h\
	..\sim_rev.h

$(INTDIR)/scp.obj :  $(SOURCE)  $(DEP_SCP_C) $(INTDIR)

# End Source File
# End Group
# End Project
################################################################################
