;
; This is a script to generate a Win32 ICI self-extracting install exe
; using NSIS - the free Nullsoft Installer System for Windows. See:
;
;     http://www.nullsoft.com/free/nsis/
;
; If you have installed NSIS, you should be able to right-click on this
; file in Explorer and select "Compile" to generate a new Win32 installer.
; We assume everything has been built before you run this. We also assume
; that ici-modules is a sibliing of ici.
;

;
; The define below is the name we use everywhere - titles, registry keys,
; etc.
;
!define NAME "ICI Programming Language"

;
; EXT is defined to be the ici-modules directory so we can pick up any
; extension modules. If this macro definition is commented out no extension
; module section will be included in this installer (which you may need to
; do it you are trying to build an installer but haven't built the extensions
; yet).
;
!define EXT "..\ici-modules"

Name "${NAME}"
OutFile "ici-v4-install.exe"

SetDateSave on
;SetOverwrite ifnewer


;
; Set the text of the component selection dialog. This has the side
; effect of enabling the component selection dialog.
;
ComponentText "This will install basic run-time support for the \
ICI Programming Language Version 4. The extension modules in \
this package are: xml, sys. There is a separate installer for the SDK."

;
; Enable and set the text for the install location dialog.
;
DirShow show
DirText "If you install in a location other than the Windows \
directory or the Windows system directory (the default), \
<that-dir> should be on your PATH and you should include \
<that-dir>\ici in your ICIPATH environment variable. \
Most files will be placed in <that-dir>\ici."
InstallDir "$SYSDIR"
InstallDirRegKey HKLM "SOFTWARE\${NAME}" ""

;
; Default section. Always executed. Other sections are only executed if
; the user selects them at install time.
;
Section ""
SetOutPath "$INSTDIR"
WriteRegStr HKLM "SOFTWARE\${NAME}" "" "$INSTDIR"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "DisplayName" "${NAME} (remove only)"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}" "UninstallString" '"$INSTDIR\ici-uninst.exe"'
WriteUninstaller "ici-uninst.exe"
SectionEnd

;
; Core language section. 
;
Section "Core Language"
SetOutPath "$INSTDIR"
File "ici.exe"
File "iciw.exe"
File "ici4.dll"
SetOutPath "$INSTDIR\ici"
File "ici4widb.dll"
File "ici4core.ici"
File "ici4core1.ici"
File "ici4core2.ici"
File "ici4core3.ici"
File "ici4widb.dll"
File "test-core.ici"
SectionEnd

!ifdef EXT
;
; Extension modules. If you can't do this because they aren't built yet
; you may need to comment out the definition of EXT above.
;
Section "Extension modules"
SetOutPath "$INSTDIR\ici"
File "/oname=ici4xml.dll" "${EXT}\xml\ici4xml.dll"
File "/oname=ici4xml.ici" "${EXT}\xml\ici4xml.ici"
File "/oname=ici4sys.dll" "${EXT}\sys\ici4sys.dll"
File "/oname=ici4net.dll" "${EXT}\net\ici4net.dll"
File "/oname=ici4net.ici" "${EXT}\net\ici4net.ici"
SectionEnd
!endif

;
; Manual section.
;
Section "Manual in PDF"
SetOutPath "$INSTDIR\ici"
File "/oname=ici.pdf" "doc\ici.pdf"
CreateShortCut "$SMPROGRAMS\ICI Programming Language Manual.lnk" "$INSTDIR\ici.pdf"
SetOutPath "$INSTDIR"
SectionEnd

;----------------------------------------------------------------------
; Uninstall stuff. Note that this stuff is logically seperate from the
; install stuff above (for obvious reasons).
;
UninstallText "This will uninstall ${NAME} from your system"

Section Uninstall
Delete "$INSTDIR\ici-uninst.exe"
;
; Core...
;
Delete "$INSTDIR\ici.exe"
Delete "$INSTDIR\iciw.exe"
Delete "$INSTDIR\ici4.dll"
Delete "$INSTDIR\ici\ici4widb.dll"
Delete "$INSTDIR\ici\ici4core.ici"
Delete "$INSTDIR\ici\ici4core1.ici"
Delete "$INSTDIR\ici\ici4core2.ici"
Delete "$INSTDIR\ici\ici4core3.ici"
Delete "$INSTDIR\ici\ici4widb.dll"
Delete "$INSTDIR\ici\test-core.ici"
;
; Manual...
;
Delete "$SMPROGRAMS\ICI Programming Language Manual.lnk"
Delete "$INSTDIR\ici\ici.pdf"

!ifdef EXT
;
; Extension modules...
;
Delete "$INSTDIR\ici\ici4xml.dll"
Delete "$INSTDIR\ici\ici4xml.ici"
Delete "$INSTDIR\ici\ici4sys.dll"
Delete "$INSTDIR\ici\ici4net.dll"
Delete "$INSTDIR\ici\ici4net.ici"
!endif

RMDir "$INSTDIR\ici"
DeleteRegKey HKLM "SOFTWARE\${NAME}"
DeleteRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
SectionEnd
