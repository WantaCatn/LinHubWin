#define MyAppName "LinHub"
#define MyAppNameZh "LinHub终端连接管理器"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "LinHub"
#define MyAppExeName "linhub.exe"

[Setup]
AppId={{8C3E2A71-6B4D-4F19-9E5A-2D1C8B0A4F33}
AppName={#MyAppNameZh}
AppVersion={#MyAppVersion}
AppVerName={#MyAppNameZh} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppCopyright=LinHub
DefaultDirName={autopf}\LinHub
DefaultGroupName={#MyAppNameZh}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=LinHub-0.2.0-win64-setup
SetupIconFile=..\resources\icons\linhub.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppNameZh}
Compression=lzma2/ultra
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=6.1sp1
UsePreviousAppDir=yes
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=no
LanguageDetectionMethod=none

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce

[Files]
Source: "..\dist\payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppNameZh}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppNameZh}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppNameZh}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppNameZh}}"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function InstalledVersion(var Version: String): Boolean;
var
  UninstallKey: String;
begin
  Result := False;
  Version := '';
  UninstallKey := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8C3E2A71-6B4D-4F19-9E5A-2D1C8B0A4F33}_is1';
  if RegQueryStringValue(HKCU, UninstallKey, 'DisplayVersion', Version) then
    Result := True
  else if RegQueryStringValue(HKLM, UninstallKey, 'DisplayVersion', Version) then
    Result := True
  else if RegQueryStringValue(HKLM32, UninstallKey, 'DisplayVersion', Version) then
    Result := True
  else if RegQueryStringValue(HKLM64, UninstallKey, 'DisplayVersion', Version) then
    Result := True;
end;

function InitializeSetup(): Boolean;
var
  Current: String;
  Msg: String;
begin
  Result := True;
  if WizardSilent then
    Exit;
  if not InstalledVersion(Current) then
    Exit;

  if Current = '{#MyAppVersion}' then
    Msg := '检测到本机已安装 {#MyAppNameZh} ' + Current + '。' + #13#10#13#10 +
           '是否重新安装当前版本？'
  else
    Msg := '检测到本机已安装 {#MyAppNameZh} ' + Current + '。' + #13#10#13#10 +
           '是否更新到 {#MyAppVersion}？';

  Result := MsgBox(Msg, mbConfirmation, MB_YESNO) = IDYES;
end;
