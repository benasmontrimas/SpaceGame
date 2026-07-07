# Build Script for The Lonely Surveyor
[CmdletBinding()]

param (
        [string]$configuration = "Release"
)

$project_directory_path = Resolve-Path (Join-Path $PSScriptRoot "..")
$bin_directory_path = Join-Path $project_directory_path "Bin"
$executable_directory_path = Join-Path $bin_directory_path $configuration
$assets_directory_path = Join-Path $project_directory_path "Assets"
$libs_directory_path = Join-Path $project_directory_path "External" "lib"

Set-Location $project_directory_path

Write-Verbose "Project Directory: $project_directory_path"
Write-Verbose "Bin Directory: $bin_directory_path"
Write-Verbose "Output Directory: $executable_directory_path"
Write-Verbose "Assets Directory: $assets_directory_path"

# Build

$build_command = "MSBuild"

try {
        Get-Command $build_command -ErrorAction Stop | Out-Null
} catch {
        Write-Error "MSBuild is required to build this project and was not found."
        exit(1)
}

$solution_path = Join-Path $project_directory_path SpaceGame.sln
msbuild -nologo /p:configuration=$configuration /p:platform=Windows $solution_path

return 0