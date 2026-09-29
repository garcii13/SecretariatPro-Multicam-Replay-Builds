#ifndef Version
  #error Version is required
#endif
#ifndef Configuration
  #define Configuration "Release"
#endif
[Setup]
AppId=SecretariatProMulticamReplay
AppName=SecretariatPro Multicam Replay
AppVersion={#Version}
AppPublisher=SecretariatPro
DefaultDirName={commonappdata}\obs-studio\plugins\secretariatpro-multicam-replay
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=..\release
OutputBaseFilename=secretariatpro-multicam-replay-{#Version}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
[Files]
Source: "..\release\{#Configuration}\secretariatpro-multicam-replay\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb"
