Name "Neko Throne"
OutFile "Neko_ThroneSetup.exe"
InstallDir $APPDATA\Neko_Throne
RequestExecutionLevel user

!include MUI2.nsh
!define MUI_ICON "res\Throne.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Welcome to Neko Throne Installer"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of Neko Throne."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Neko_Throne.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Neko Throne"
!addplugindir .\script\

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

UninstallText "This will uninstall Neko Throne. Do you wish to continue?"
UninstallIcon "res\ThroneDel.ico"

Function .onInit
  ReadRegStr $R0 HKCU "Software\Neko_Throne" "InstallPath"
  StrCmp $R0 "" +2
  StrCpy $INSTDIR $R0
FunctionEnd

!macro AbortOnRunningApp EXEName
  killModule:
  FindProcDLL::FindProc ${EXEName}
  Pop $R0
  IntCmp $R0 1 0 notRunning
    FindProcDLL::KillProc ${EXEName}
    Sleep 1000
    Goto killModule
  notRunning:
!macroend

Section "Install"
  SetOutPath "$INSTDIR"
  SetOverwrite on

  !insertmacro AbortOnRunningApp "$INSTDIR\Neko_Throne.exe"

  File /r ".\deployment\windows64\NekoCore.exe"
  File /r ".\deployment\windows64\Neko_Throne.exe"
  File /r ".\deployment\windows64\updater.exe"

  CreateShortcut "$desktop\Neko Throne.lnk" "$instdir\Neko_Throne.exe"
  CreateShortcut "$SMPROGRAMS\Neko Throne.lnk" "$INSTDIR\Neko_Throne.exe" "" "$INSTDIR\Neko_Throne.exe" 0

  WriteRegStr HKCU "Software\Neko_Throne" "InstallPath" "$INSTDIR"
  
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne" "DisplayName" "Neko Throne"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "Uninstall"

  !insertmacro AbortOnRunningApp "$INSTDIR\Neko_Throne.exe"

  Delete "$SMPROGRAMS\Neko Throne.lnk"
  Delete "$desktop\Neko Throne.lnk"
  RMDir "$SMPROGRAMS\Neko Throne"

  RMDir /r "$INSTDIR"

  Delete "$INSTDIR\uninstall.exe"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Neko_Throne"
SectionEnd