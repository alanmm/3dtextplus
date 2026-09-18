; Instalador do 3D Text+ (Inno Setup 6 - https://jrsoftware.org/isdl.php).
; Copia o .scr para {sys} (System32), onde o Windows procura proteções de
; tela pra listar em Configurações > Personalização > Tela de bloqueio >
; Proteção de tela, cria atalhos no menu Iniciar e oferece ativar como
; proteção de tela atual ao final.
;
; Build precisa existir antes de compilar: rode primeiro
;   mingw32-make -f build/Makefile release
; a partir da raiz do projeto, depois compile este script (botão Compile
; no Inno Setup, ou `ISCC.exe installer\3DTextPlus.iss` a partir da raiz).

#define MyAppName "3D Text+"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Alan Maziero"
#define MyAppURL "https://github.com/alanmm/3dtextplus"
#define MyAppExeName "3DTextPlus.scr"

[Setup]
AppId={{ABDA4B7B-B5DF-4558-A057-EDDA53868490}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=3DTextPlus-Setup-{#MyAppVersion}
SetupIconFile=..\res\icon\Icone_3dText.ico
UninstallDisplayIcon={sys}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "setactive"; Description: "{cm:SetActiveTask}"; GroupDescription: "{cm:AdditionalTasks}"

[Files]
Source: "..\dist\3DTextPlus.scr"; DestDir: "{sys}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}\{cm:ConfigureIcon}"; Filename: "{sys}\{#MyAppExeName}"; Parameters: "/c"; IconFilename: "{sys}\{#MyAppExeName}"
Name: "{autoprograms}\{#MyAppName}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Control Panel\Desktop"; ValueType: string; ValueName: "SCRNSAVE.EXE"; ValueData: "{sys}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: setactive
Root: HKCU; Subkey: "Control Panel\Desktop"; ValueType: string; ValueName: "ScreenSaveActive"; ValueData: "1"; Tasks: setactive

[Run]
Filename: "{sys}\{#MyAppExeName}"; Parameters: "/c"; Description: "{cm:OpenConfigAfter}"; Flags: postinstall nowait skipifsilent unchecked

[Messages]
brazilianportuguese.WelcomeLabel2=Isso vai instalar [name/ver] no seu computador.%n%nO instalador copia o arquivo de proteção de tela para a pasta do sistema do Windows - depois disso, "3D Text+" aparece na lista de proteções de tela em Configurações > Personalização.
english.WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe installer copies the screensaver file into the Windows system folder - after that, "3D Text+" shows up in the screen saver list under Settings > Personalization.

[CustomMessages]
brazilianportuguese.SetActiveTask=Ativar 3D Text+ como proteção de tela atual
brazilianportuguese.AdditionalTasks=Tarefas adicionais:
brazilianportuguese.ConfigureIcon=Configurar 3D Text+
brazilianportuguese.OpenConfigAfter=Abrir as configurações do 3D Text+
english.SetActiveTask=Set 3D Text+ as the current screen saver
english.AdditionalTasks=Additional tasks:
english.ConfigureIcon=Configure 3D Text+
english.OpenConfigAfter=Open 3D Text+ settings
