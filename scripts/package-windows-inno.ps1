param(
    [string]$BuildDir = "",
    [string]$OutputDir = "",
    [string]$WorkDir = "",
    [string]$IsccPath = "",
    [string]$AppVersion = "",
    [switch]$GenerateOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path (Join-Path $ScriptDir "..")).Path

function Get-FullPath {
    param([string]$Path)

    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-InsideRoot {
    param(
        [string]$Path,
        [string]$Description
    )

    $FullPath = Get-FullPath $Path
    $RootWithSeparator = $RootDir.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

    if (-not $FullPath.StartsWith($RootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must be inside the repository: $FullPath"
    }

    return $FullPath
}

function Find-Iscc {
    param([string]$PreferredPath)

    if (-not [string]::IsNullOrWhiteSpace($PreferredPath)) {
        return (Resolve-Path -LiteralPath $PreferredPath).Path
    }

    $Command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }

    $Candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 5\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 5\ISCC.exe")
    )

    foreach ($Candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($Candidate) -and (Test-Path -LiteralPath $Candidate)) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    throw "ISCC.exe was not found. Install Inno Setup or pass -IsccPath. This script does not rebuild Mycel."
}

function Read-AppVersion {
    $CMakeFile = Join-Path $RootDir "CMakeLists.txt"
    $CMakeText = Get-Content -LiteralPath $CMakeFile -Raw

    if ($CMakeText -match "project\s*\(\s*Mycel\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3})") {
        return $Matches[1]
    }

    throw "Could not read the Mycel version from CMakeLists.txt."
}

function Copy-IfExists {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
}

function Escape-IssString {
    param([string]$Value)

    return $Value.Replace('"', '""')
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RootDir "build-windows-msvc"
}

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    $WorkDir = Join-Path $RootDir "build-inno-installer"
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $WorkDir "output"
}

if ([string]::IsNullOrWhiteSpace($AppVersion)) {
    $AppVersion = Read-AppVersion
}

$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$WorkDir = Assert-InsideRoot $WorkDir "WorkDir"
$StagingDir = Assert-InsideRoot (Join-Path $WorkDir "payload") "StagingDir"
$OutputDir = Get-FullPath $OutputDir
$IssPath = Assert-InsideRoot (Join-Path $WorkDir "mycel.iss") "Inno script path"

$ExePath = Join-Path $BuildDir "mycel.exe"
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "Built binary was not found: $ExePath. Run the Windows build first; this script does not rebuild Mycel."
}

$IconPath = Join-Path $RootDir "assets\mycel.ico"
if (-not (Test-Path -LiteralPath $IconPath)) {
    throw "Installer icon was not found: $IconPath"
}

$RequiredRuntimeFiles = @(
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Multimedia.dll",
    "Qt6MultimediaWidgets.dll",
    "Qt6Widgets.dll",
    "multimedia\ffmpegmediaplugin.dll",
    "multimedia\windowsmediaplugin.dll",
    "platforms\qwindows.dll"
)

foreach ($RequiredFile in $RequiredRuntimeFiles) {
    $RequiredPath = Join-Path $BuildDir $RequiredFile
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required runtime file was not found: $RequiredPath. Deploy the built output first; this script does not run windeployqt or rebuild Mycel."
    }
}

if (Test-Path -LiteralPath $StagingDir) {
    Remove-Item -LiteralPath $StagingDir -Recurse -Force
}

New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Copy-Item -LiteralPath $ExePath -Destination $StagingDir -Force
Copy-Item -Path (Join-Path $BuildDir "*.dll") -Destination $StagingDir -Force

$RuntimeDirectories = @(
    "generic",
    "iconengines",
    "imageformats",
    "multimedia",
    "networkinformation",
    "platforms",
    "styles",
    "tls",
    "translations"
)

foreach ($RuntimeDirectory in $RuntimeDirectories) {
    Copy-IfExists (Join-Path $BuildDir $RuntimeDirectory) $StagingDir
}

Copy-IfExists (Join-Path $RootDir "README.md") $StagingDir
Copy-IfExists (Join-Path $RootDir "README.ja.md") $StagingDir
Copy-Item -LiteralPath $IconPath -Destination $StagingDir -Force

$EscapedPayloadDir = Escape-IssString $StagingDir
$EscapedOutputDir = Escape-IssString $OutputDir
$EscapedAppVersion = Escape-IssString $AppVersion
$EscapedIconPath = Escape-IssString $IconPath

$IssContent = @"
#define MyAppName "Mycel"
#define MyAppVersion "$EscapedAppVersion"
#define MyAppExeName "mycel.exe"
#define PayloadDir "$EscapedPayloadDir"

[Setup]
AppId={{865C6B07-518B-4C06-9E38-B6BF0E57CBA1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={localappdata}\Programs\Mycel
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=$EscapedOutputDir
OutputBaseFilename=MycelSetup-{#MyAppVersion}
SetupIconFile=$EscapedIconPath
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\mycel.ico

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\mycel.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\mycel.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "modifypath"; Description: "Add Mycel to the user PATH"; GroupDescription: "Command line:"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
const
  EnvironmentKey = 'Environment';
  EnvironmentValue = 'Path';
  MycelHwndBroadcast = 65535;
  MycelWmSettingChange = 26;
  MycelSmtoAbortIfHung = 2;

function SendMessageTimeout(hWnd: HWND; Msg: UINT; wParam: Longint; lParam: String;
  fuFlags: UINT; uTimeout: UINT; var lpdwResult: DWORD): Longint;
  external 'SendMessageTimeoutW@user32.dll stdcall';

function NormalizePathForCompare(Path: string): string;
begin
  Result := Lowercase(Trim(Path));
  while (Length(Result) > 3) and (Copy(Result, Length(Result), 1) = '\') do
    Delete(Result, Length(Result), 1);
end;

function PathContainsDir(PathValue: string; Dir: string): Boolean;
var
  Remaining: string;
  Item: string;
  Separator: Integer;
  NormalizedDir: string;
begin
  Result := False;
  Remaining := PathValue;
  NormalizedDir := NormalizePathForCompare(Dir);

  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator > 0 then
    begin
      Item := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end
    else
    begin
      Item := Remaining;
      Remaining := '';
    end;

    if NormalizePathForCompare(Item) = NormalizedDir then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function RemoveDirFromPath(PathValue: string; Dir: string): string;
var
  Remaining: string;
  Item: string;
  Separator: Integer;
  NormalizedDir: string;
begin
  Result := '';
  Remaining := PathValue;
  NormalizedDir := NormalizePathForCompare(Dir);

  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator > 0 then
    begin
      Item := Copy(Remaining, 1, Separator - 1);
      Delete(Remaining, 1, Separator);
    end
    else
    begin
      Item := Remaining;
      Remaining := '';
    end;

    if (Trim(Item) <> '') and (NormalizePathForCompare(Item) <> NormalizedDir) then
    begin
      if Result <> '' then
        Result := Result + ';';
      Result := Result + Item;
    end;
  end;
end;

procedure BroadcastEnvironmentChange;
var
  ResultCode: DWORD;
begin
  SendMessageTimeout(MycelHwndBroadcast, MycelWmSettingChange, 0, 'Environment',
    MycelSmtoAbortIfHung, 5000, ResultCode);
end;

procedure AddAppDirToPath;
var
  PathValue: string;
  AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKCU, EnvironmentKey, EnvironmentValue, PathValue) then
    PathValue := '';

  if PathContainsDir(PathValue, AppDir) then
    Exit;

  if Trim(PathValue) = '' then
    PathValue := AppDir
  else
    PathValue := PathValue + ';' + AppDir;

  RegWriteStringValue(HKCU, EnvironmentKey, EnvironmentValue, PathValue);
  BroadcastEnvironmentChange;
end;

procedure RemoveAppDirFromPath;
var
  PathValue: string;
  NewPathValue: string;
  AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  if not RegQueryStringValue(HKCU, EnvironmentKey, EnvironmentValue, PathValue) then
    Exit;

  NewPathValue := RemoveDirFromPath(PathValue, AppDir);
  if NewPathValue <> PathValue then
  begin
    RegWriteStringValue(HKCU, EnvironmentKey, EnvironmentValue, NewPathValue);
    BroadcastEnvironmentChange;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('modifypath') then
    AddAppDirToPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveAppDirFromPath;
end;
"@

Set-Content -LiteralPath $IssPath -Value $IssContent -Encoding UTF8

Write-Host "Prepared Inno Setup script: $IssPath"
Write-Host "Staged existing build output: $StagingDir"

if ($GenerateOnly) {
    Write-Host "GenerateOnly was specified; installer compilation was skipped."
    exit 0
}

$ResolvedIsccPath = Find-Iscc $IsccPath
& $ResolvedIsccPath $IssPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$InstallerPath = Join-Path $OutputDir "MycelSetup-$AppVersion.exe"
Write-Host "Created installer: $InstallerPath"
