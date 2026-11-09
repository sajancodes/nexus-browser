#Requires -Version 5.1
<#
.SYNOPSIS
  Builds NEXUS with CMake + Ninja using the MSVC x64 toolchain.

.DESCRIPTION
  Builds the React frontend with pnpm, imports the Visual Studio developer
  environment, then configures and builds the requested preset.

.PARAMETER Preset
  CMake preset to build: windows-x64-release (default) or windows-x64-debug.

.PARAMETER Target
  Optional CMake target. Defaults to the whole application and its tests.

.PARAMETER SkipFrontend
  Do not rebuild the React shell. Native-only iteration.

.PARAMETER ConfigureOnly
  Configure without building.
#>
[CmdletBinding()]
param(
  [ValidateSet('windows-x64-release', 'windows-x64-debug')]
  [string]$Preset = 'windows-x64-release',

  [string]$Target = 'all',

  [switch]$SkipFrontend,

  [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$frontendDir = Join-Path $repoRoot 'apps\desktop\frontend'

function Invoke-FrontendBuild {
  Push-Location $repoRoot
  try {
    if (-not (Get-Command corepack -ErrorAction SilentlyContinue)) {
      throw 'corepack was not found. Install Node.js 20 or newer.'
    }

    if (-not (Test-Path (Join-Path $repoRoot 'node_modules'))) {
      Write-Host 'Installing frontend dependencies...' -ForegroundColor Cyan
      & corepack pnpm install --frozen-lockfile
      if ($LASTEXITCODE -ne 0) {
        throw "pnpm install failed with exit code $LASTEXITCODE."
      }
    }

    Write-Host 'Building React shell...' -ForegroundColor Cyan
    & corepack pnpm --dir $frontendDir build
    if ($LASTEXITCODE -ne 0) {
      throw "Frontend build failed with exit code $LASTEXITCODE."
    }
  } finally {
    Pop-Location
  }
}

if (-not $SkipFrontend) {
  Invoke-FrontendBuild
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
  throw 'vswhere.exe not found. Install Visual Studio 2022 Build Tools with the C++ workload.'
}

$vsPath = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
if (-not $vsPath) {
  throw 'No Visual Studio installation with the MSVC x64 toolchain was found.'
}

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
  throw "vcvars64.bat not found at $vcvars"
}

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ninja = (Get-Command ninja -ErrorAction Stop).Source
$frontendOption = if ($SkipFrontend) { 'OFF' } else { 'ON' }

$commands = @(
  ('"' + $cmake + '" --preset ' + $Preset + ' -DNEXUS_BUILD_FRONTEND=' + $frontendOption)
)
if (-not $ConfigureOnly) {
  $commands += ('"' + $cmake + '" --build --preset ' + $Preset + ' --target ' + $Target)
}

$commandLine = 'call "' + $vcvars + '" >nul && ' + ($commands -join ' && ')
Write-Host "NEXUS build: $Preset -> $Target" -ForegroundColor Cyan
& cmd.exe /d /s /c $commandLine
if ($LASTEXITCODE -ne 0) {
  throw "Build failed with exit code $LASTEXITCODE."
}

Write-Host 'Build succeeded.' -ForegroundColor Green
