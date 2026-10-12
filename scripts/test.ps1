# Builds the unit-test DLL and runs it with vstest. Exit code is non-zero on any failure.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
$vstest = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\Extensions\TestPlatform\vstest.console.exe'
& $msbuild "$root\Tests\WANUtility.Tests.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Test build failed ($LASTEXITCODE)" }
& $vstest "$root\Tests\x64\Debug\WANUtility.Tests.dll" /Platform:x64
exit $LASTEXITCODE
