param(
    [string]$BuildDir = "",
    [string]$BuildType = "Release",
    [string]$CMakePrefixPath = "",
    [string]$Generator = "",
    [string]$CMakeBin = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..")
$BuildDirWasProvided = -not [string]::IsNullOrWhiteSpace($BuildDir)

function Add-PathIfExists {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    $ResolvedPath = (Resolve-Path $Path).Path
    $PathEntries = $env:PATH -split [System.IO.Path]::PathSeparator
    if ($PathEntries -notcontains $ResolvedPath) {
        $env:PATH = $ResolvedPath + [System.IO.Path]::PathSeparator + $env:PATH
    }
}

function Find-QtPrefixPath {
    if (-not (Test-Path "C:\Qt")) {
        return ""
    }

    $QtKits = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue
        } |
        Where-Object {
            Test-Path (Join-Path $_.FullName "lib\cmake\Qt6\Qt6Config.cmake")
        } |
        Sort-Object FullName -Descending

    if ($QtKits.Count -eq 0) {
        return ""
    }

    return $QtKits[0].FullName
}

function Find-CMake {
    param([string]$PreferredCMake)

    if (-not [string]::IsNullOrWhiteSpace($PreferredCMake)) {
        return (Resolve-Path $PreferredCMake).Path
    }

    $QtCMake = Get-ChildItem "C:\Qt\Tools" -Directory -Filter "CMake*" -ErrorAction SilentlyContinue |
        ForEach-Object {
            Join-Path $_.FullName "bin\cmake.exe"
        } |
        Where-Object {
            Test-Path $_
        } |
        Sort-Object -Descending |
        Select-Object -First 1

    if ($QtCMake) {
        return $QtCMake
    }

    $PathCMake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($PathCMake) {
        return $PathCMake.Source
    }

    return "cmake"
}

function Find-WinDeployQt {
    param([string]$QtPrefixPath)

    if (-not [string]::IsNullOrWhiteSpace($QtPrefixPath)) {
        $Candidate = Join-Path $QtPrefixPath "bin\windeployqt.exe"
        if (Test-Path -LiteralPath $Candidate) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    $Command = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }

    return ""
}

function Find-BuiltExecutable {
    param(
        [string]$BuildDir,
        [string]$BuildType
    )

    $Candidates = @(
        (Join-Path $BuildDir "mycel.exe"),
        (Join-Path (Join-Path $BuildDir $BuildType) "mycel.exe")
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path -LiteralPath $Candidate) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    return ""
}

function Find-VcVars64 {
    $VcVars64 = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "vcvars64.bat" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($VcVars64) {
        return $VcVars64.FullName
    }

    return ""
}

function Import-VcVars64 {
    $Cl = Get-Command cl -ErrorAction SilentlyContinue
    if ($Cl) {
        return
    }

    $VcVars64 = Find-VcVars64
    if ([string]::IsNullOrWhiteSpace($VcVars64)) {
        return
    }

    $EnvironmentLines = & cmd /c "call `"$VcVars64`" >nul && set"
    $VcPathLine = $EnvironmentLines |
        Where-Object {
            $_ -match "^(Path|PATH)=" -and $_ -match "\\VC\\Tools\\MSVC\\.*\\bin\\Host"
        } |
        Select-Object -First 1

    foreach ($Line in $EnvironmentLines) {
        $Parts = $Line -split "=", 2
        if ($Parts.Count -eq 2) {
            if ($Parts[0] -ieq "Path") {
                continue
            }

            Set-Item -Path "Env:\$($Parts[0])" -Value $Parts[1]
        }
    }

    if ($VcPathLine) {
        $env:Path = ($VcPathLine -split "=", 2)[1]
    }

    $Cl = Get-Command cl -ErrorAction SilentlyContinue
    if (-not $Cl) {
        throw "MSVC compiler cl.exe was not found after running $VcVars64."
    }
}

if ([string]::IsNullOrWhiteSpace($CMakePrefixPath) -and -not $env:CMAKE_PREFIX_PATH) {
    $DetectedQtPrefixPath = Find-QtPrefixPath
    if (-not [string]::IsNullOrWhiteSpace($DetectedQtPrefixPath)) {
        $CMakePrefixPath = $DetectedQtPrefixPath
    }
}

if ($CMakePrefixPath -match "mingw|msvc") {
    $QtRoot = Split-Path -Parent (Split-Path -Parent $CMakePrefixPath)
    $QtToolsDir = Join-Path $QtRoot "Tools"

    Add-PathIfExists (Join-Path $QtToolsDir "Ninja")
}

if ($CMakePrefixPath -match "mingw") {
    $MingwBin = Get-ChildItem $QtToolsDir -Directory -Filter "mingw*_64" -ErrorAction SilentlyContinue |
        ForEach-Object {
            Join-Path $_.FullName "bin"
        } |
        Where-Object {
            Test-Path (Join-Path $_ "g++.exe")
        } |
        Sort-Object -Descending |
        Select-Object -First 1

    if ($MingwBin) {
        Add-PathIfExists $MingwBin
    }

    if ([string]::IsNullOrWhiteSpace($Generator)) {
        $Generator = "Ninja"
    }
} elseif ($CMakePrefixPath -match "msvc") {
    Import-VcVars64
    Add-PathIfExists (Join-Path $QtToolsDir "Ninja")

    if ([string]::IsNullOrWhiteSpace($Generator)) {
        $Generator = "Ninja"
    }

    if ($Generator -eq "Ninja") {
        $env:CC = "cl"
        $env:CXX = "cl"
    }
}

$CMakeExe = Find-CMake $CMakeBin
$CMakeDir = Split-Path -Parent $CMakeExe
Add-PathIfExists $CMakeDir

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    if ($CMakePrefixPath -match "mingw") {
        $BuildDir = Join-Path $RootDir "build-mingw"
    } elseif ($CMakePrefixPath -match "msvc") {
        $BuildDir = Join-Path $RootDir "build-windows-msvc"
    } else {
        $BuildDir = Join-Path $RootDir "build"
    }
}

$CMakeCache = Join-Path $BuildDir "CMakeCache.txt"
if (-not $BuildDirWasProvided -and -not [string]::IsNullOrWhiteSpace($Generator) -and (Test-Path $CMakeCache)) {
    $CachedGenerator = Select-String -Path $CMakeCache -Pattern "^CMAKE_GENERATOR:INTERNAL=(.+)$" -ErrorAction SilentlyContinue |
        Select-Object -First 1

    if ($CachedGenerator -and $CachedGenerator.Matches[0].Groups[1].Value -ne $Generator) {
        if ($CMakePrefixPath -match "mingw") {
            $BuildDir = Join-Path $RootDir "build-mingw"
        } elseif ($CMakePrefixPath -match "msvc") {
            $BuildDir = Join-Path $RootDir "build-windows-msvc"
        } else {
            $SafeGeneratorName = ($Generator -replace "[^A-Za-z0-9]+", "-").Trim("-").ToLowerInvariant()
            $BuildDir = Join-Path $RootDir "build-$SafeGeneratorName"
        }
    }
}

$ConfigureArgs = @(
    "-S", $RootDir,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $ConfigureArgs += @("-G", $Generator)
}

if (-not [string]::IsNullOrWhiteSpace($CMakePrefixPath)) {
    $ConfigureArgs += "-DCMAKE_PREFIX_PATH=$CMakePrefixPath"
} elseif ($env:CMAKE_PREFIX_PATH) {
    $ConfigureArgs += "-DCMAKE_PREFIX_PATH=$env:CMAKE_PREFIX_PATH"
}

& $CMakeExe @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $CMakeExe --build $BuildDir --config $BuildType
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$BuiltExe = Find-BuiltExecutable $BuildDir $BuildType
if (-not [string]::IsNullOrWhiteSpace($BuiltExe)) {
    $WinDeployQt = Find-WinDeployQt $CMakePrefixPath
    if (-not [string]::IsNullOrWhiteSpace($WinDeployQt)) {
        $DeployArgs = @()
        if ($BuildType -ieq "Debug") {
            $DeployArgs += "--debug"
        } else {
            $DeployArgs += "--release"
        }
        $DeployArgs += $BuiltExe
        & $WinDeployQt @DeployArgs
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } else {
        Write-Warning "windeployqt.exe was not found. The executable was built, but Qt runtime files were not deployed."
    }
}
