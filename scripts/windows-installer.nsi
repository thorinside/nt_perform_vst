; NT Perform VST3 Installer
; Usage: makensis /DPRODUCT_VERSION=1.2.3 windows-installer.nsi

!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.0.0"
!endif

!define PRODUCT_NAME    "NT Perform"
!define PRODUCT_ID      "NTPerform"
!define VST3_DEST       "$COMMONFILES\VST3"
!define UNINST_KEY      "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "NTPerform-${PRODUCT_VERSION}-Windows.exe"
InstallDir "${VST3_DEST}"
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show

;------------------------------------------------------------
Section "VST3 Plugin" SecVST3
  SetOutPath "${VST3_DEST}"
  File /r "NTPerform.vst3"

  ; Write uninstaller
  CreateDirectory "$PROGRAMFILES\${PRODUCT_NAME}"
  WriteUninstaller "$PROGRAMFILES\${PRODUCT_NAME}\Uninstall.exe"

  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName"     "${PRODUCT_NAME}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion"  "${PRODUCT_VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher"       "Nosuch"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" \
    '"$PROGRAMFILES\${PRODUCT_NAME}\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

;------------------------------------------------------------
Section "Uninstall"
  RMDir /r "${VST3_DEST}\NTPerform.vst3"
  Delete   "$PROGRAMFILES\${PRODUCT_NAME}\Uninstall.exe"
  RMDir    "$PROGRAMFILES\${PRODUCT_NAME}"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
