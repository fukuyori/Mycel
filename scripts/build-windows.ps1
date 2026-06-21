param(
    [string]$BuildDir = "",
    [string]$BuildType = "Release",
    [string]$CMakePrefixPath = "",
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..")

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RootDir "build"
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

cmake @ConfigureArgs
cmake --build $BuildDir --config $BuildType

