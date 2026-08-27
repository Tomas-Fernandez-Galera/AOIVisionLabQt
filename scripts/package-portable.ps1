param(
    [string]$QtRoot = "C:\Qt\6.10.2\mingw_64",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot "build\release-portable"
$packageName = "AOIVisionLabQt-$Version-Windows-x64"
$distributionRoot = Join-Path $projectRoot "dist"
$packageDirectory = Join-Path $distributionRoot $packageName
$archivePath = Join-Path $distributionRoot "$packageName.zip"
$qmake = Join-Path $QtRoot "bin\qmake.exe"
$deployQt = Join-Path $QtRoot "bin\windeployqt.exe"
$mingwBin = "C:\Qt\Tools\mingw1310_64\bin"
$opencvBin = Join-Path $projectRoot "vcpkg_installed\x64-mingw-dynamic\bin"

if (!(Test-Path -LiteralPath $qmake) -or !(Test-Path -LiteralPath $deployQt)) {
    throw "Qt tools were not found under $QtRoot"
}

# Build Release in an isolated directory so packaging never modifies the
# developer's Qt Creator debug build.
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$previousPath = $env:Path
try {
    $env:Path = "$mingwBin;$QtRoot\bin;$opencvBin;$previousPath"
    Push-Location $buildDirectory
    & $qmake "$projectRoot\AOIVisionLabQt.pro" "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) { throw "qmake failed" }
    & mingw32-make -j4
    if ($LASTEXITCODE -ne 0) { throw "Release compilation failed" }
    Pop-Location

    # Only the explicitly named version directory is replaced. Older packages
    # remain available for comparison and rollback.
    if (Test-Path -LiteralPath $packageDirectory) {
        Remove-Item -LiteralPath $packageDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packageDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDirectory "AOIVisionLabQt.exe") `
              -Destination $packageDirectory
    & $deployQt --release --no-translations --compiler-runtime `
        (Join-Path $packageDirectory "AOIVisionLabQt.exe")
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

    # OpenCV is installed dynamically by vcpkg; copy only the modules linked by
    # the executable plus their image-codec runtime dependencies.
    $opencvDlls = @(
        "libopencv_calib3d4.dll", "libopencv_features2d4.dll",
        "libopencv_imgcodecs4.dll", "libopencv_imgproc4.dll",
        "libopencv_flann4.dll", "libopencv_core4.dll",
        "libjpeg-62.dll", "libpng16.dll", "libzlib1.dll", "libturbojpeg.dll"
    )
    foreach ($dll in $opencvDlls) {
        Copy-Item -LiteralPath (Join-Path $opencvBin $dll) -Destination $packageDirectory
    }

    Copy-Item -LiteralPath (Join-Path $projectRoot "test-images") `
              -Destination $packageDirectory -Recurse
    $automationDirectory = Join-Path $packageDirectory "automation"
    New-Item -ItemType Directory -Force -Path $automationDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot "automation\aoi_mcp_server.py"), `
                           (Join-Path $projectRoot "automation\mcp-config-example.json") `
              -Destination $automationDirectory
    Copy-Item -LiteralPath (Join-Path $projectRoot "README.md"), `
                           (Join-Path $projectRoot "LICENSE"), `
                           (Join-Path $projectRoot "TRADEMARKS.md") `
              -Destination $packageDirectory

    if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath -Force }
    Compress-Archive -LiteralPath $packageDirectory -DestinationPath $archivePath `
                     -CompressionLevel Optimal
}
finally {
    if ((Get-Location).Path -eq $buildDirectory) { Pop-Location }
    $env:Path = $previousPath
}

Get-Item -LiteralPath $archivePath
