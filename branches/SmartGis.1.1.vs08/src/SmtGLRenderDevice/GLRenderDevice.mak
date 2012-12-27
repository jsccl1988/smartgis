# Microsoft Developer Studio Generated NMAKE File, Based on GLRenderDevice.dsp
!IF "$(CFG)" == ""
CFG=GLRenderDevice - Win32 Debug
!MESSAGE No configuration specified. Defaulting to GLRenderDevice - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "GLRenderDevice - Win32 Release" && "$(CFG)" != "GLRenderDevice - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GLRenderDevice.mak" CFG="GLRenderDevice - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GLRenderDevice - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "GLRenderDevice - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "GLRenderDevice - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\GLRenderDevice.dll"


CLEAN :
	-@erase "$(INTDIR)\Camera.obj"
	-@erase "$(INTDIR)\ExtInterface.obj"
	-@erase "$(INTDIR)\GLRenderDevice.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\GLRenderDevice.dll"
	-@erase "$(OUTDIR)\GLRenderDevice.exp"
	-@erase "$(OUTDIR)\GLRenderDevice.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLRENDERDEVICE_EXPORTS" /Fp"$(INTDIR)\GLRenderDevice.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\GLRenderDevice.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\GLRenderDevice.pdb" /machine:I386 /out:"$(OUTDIR)\GLRenderDevice.dll" /implib:"$(OUTDIR)\GLRenderDevice.lib" 
LINK32_OBJS= \
	"$(INTDIR)\Camera.obj" \
	"$(INTDIR)\ExtInterface.obj" \
	"$(INTDIR)\GLRenderDevice.obj"

"$(OUTDIR)\GLRenderDevice.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "GLRenderDevice - Win32 Debug"

OUTDIR=E:\程序\项目\CCL_3DEngine\bin
INTDIR=.\Debug
# Begin Custom Macros
OutDir=E:\程序\项目\CCL_3DEngine\bin
# End Custom Macros

ALL : "$(OUTDIR)\GLRenderDevice.dll"


CLEAN :
	-@erase "$(INTDIR)\Camera.obj"
	-@erase "$(INTDIR)\ExtInterface.obj"
	-@erase "$(INTDIR)\GLRenderDevice.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\GLRenderDevice.dll"
	-@erase "$(OUTDIR)\GLRenderDevice.exp"
	-@erase "$(OUTDIR)\GLRenderDevice.ilk"
	-@erase "$(OUTDIR)\GLRenderDevice.lib"
	-@erase "$(OUTDIR)\GLRenderDevice.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GLRENDERDEVICE_EXPORTS" /Fp"$(INTDIR)\GLRenderDevice.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\GLRenderDevice.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\GLRenderDevice.pdb" /debug /machine:I386 /out:"$(OUTDIR)\GLRenderDevice.dll" /implib:"$(OUTDIR)\GLRenderDevice.lib" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\Camera.obj" \
	"$(INTDIR)\ExtInterface.obj" \
	"$(INTDIR)\GLRenderDevice.obj"

"$(OUTDIR)\GLRenderDevice.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("GLRenderDevice.dep")
!INCLUDE "GLRenderDevice.dep"
!ELSE 
!MESSAGE Warning: cannot find "GLRenderDevice.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "GLRenderDevice - Win32 Release" || "$(CFG)" == "GLRenderDevice - Win32 Debug"
SOURCE=.\Camera.cpp

"$(INTDIR)\Camera.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ExtInterface.cpp

"$(INTDIR)\ExtInterface.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\GLRenderDevice.cpp

"$(INTDIR)\GLRenderDevice.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

