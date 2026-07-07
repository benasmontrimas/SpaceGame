# Build Script for The Lonely Surveyor
[CmdletBinding()]

param (
        [string]$configuration = "Release"
)

$project_directory_path = Resolve-Path (Join-Path $PSScriptRoot "..")
$bin_directory_path = Join-Path $project_directory_path "Bin"
$executable_directory_path = Join-Path $bin_directory_path $configuration
$assets_directory_path = Join-Path $project_directory_path "Assets"
$external_directory_path = Join-Path $project_directory_path "External"
$libs_directory_path = Join-Path $external_directory_path "lib"

Push-Location $project_directory_path

Write-Verbose "Project Directory: $project_directory_path"
Write-Verbose "Bin Directory: $bin_directory_path"
Write-Verbose "Output Directory: $executable_directory_path"
Write-Verbose "Assets Directory: $assets_directory_path"

# Build

# Assets

function GetPathEnd {
        [CmdletBinding()]
        param (
                [Parameter(Mandatory)]
                [string]$path
        )

        Split-Path -Path $path -Leaf
}

# Copy Assets

function CopyAssetDirectory {
        [CmdletBinding()]
        param (
                [Parameter(Mandatory)]
                [string]$directory,
                [Parameter(Mandatory)]
                [string]$output_path
        )

        Write-Verbose "Copying directory: $directory"

        [int]$modified_file_count = 0

        # Check the directory we want to copy from exists
        if (-Not (Test-Path "$directory")) {
                Write-Warning "CopyAssetDirectory called on non exising directory: $directory"
                return $modified_file_count
        }

        $included_asset_file_types = @("*.ktx", "*.ttf", "*.obj", "*.slang", "*.wav", "*.mp3")

        # Write-Output $directory

        # Check if directory has any files we care about
        $all_sub_files = Get-ChildItem "$directory" -Include $included_asset_file_types -Name -Recurse

        if ($all_sub_files.Count -eq 0) {
                Write-Verbose "No asset files in directory: $directory"
                return 0
        }

        if (-Not (Test-Path $output_path)) {
                Write-Verbose "Creating output directory: $output_path"
                New-Item -Path $output_path -ItemType "Directory" | Out-Null
        }

        $sub_files = Get-ChildItem "$directory" -Include $included_asset_file_types -Name

        $i = 0
        foreach ($sub_file in $sub_files) {
                $i = $i + 1
                $progress_count = $sub_files.Count
                $progress_percent = ($i / $progress_count) * 100
                Write-Progress -Activity "Copying $(GetPathEnd $directory)" -Status "$i/$progress_count" -PercentComplete $progress_percent

                # Write-Output $sub_file
                $input_file_path = Join-Path $directory $sub_file
                $output_file_path = Join-Path $output_path $sub_file

                if (Test-Path $output_file_path) {
                        $input_file = Get-Item $input_file_path
                        $output_file = Get-Item $output_file_path

                        if (-Not ($output_file.LastWriteTime -eq $input_file.LastWriteTime)) {
                                [int]$modified_file_count = $modified_file_count + 1
                                Write-Verbose "Copying file: `n`tFrom: $input_file_path`n`tTo:$output_file_path"
                                Copy-Item $input_file_path $output_file_path | Out-Null
                        } else {
                                Write-Verbose "File is up-to date: $output_file_path"
                        }

                } else {
                        Write-Verbose "Copying file: `n`tFrom: $input_file_path`n`tTo:$output_file_path"
                        Copy-Item $input_file_path $output_file_path | Out-Null

                        [int]$modified_file_count = $modified_file_count + 1
                }
        }

        $sub_directories = Get-ChildItem "$directory" -Directory

        foreach ($sub_directory in $sub_directories) {
                $sub_directory_output_path = Join-Path "$output_path" (GetPathEnd "$sub_directory")
                $sub_directory_input_path = Join-Path "$directory" (GetPathEnd "$sub_directory")
                $directory_sub_file_count = (CopyAssetDirectory -directory "$sub_directory_input_path" -output_path "$sub_directory_output_path")
                [int]$modified_file_count = ($modified_file_count + $directory_sub_file_count)
        }

        return [int]$modified_file_count
}

"Copying Assets..."


$assets_directory = Get-Item $assets_directory_path
$assets_output_directory = Join-Path $executable_directory_path (GetPathEnd $assets_directory)

$files_copied = CopyAssetDirectory "$assets_directory" "$assets_output_directory"

"Successfully copied $files_copied modified files"

$lib_files = Get-ChildItem $libs_directory_path
foreach ($lib_file in $lib_files) {
        if ($lib_file.Extension -eq ".dll") {
                $lib_file_path = Join-Path $libs_directory_path $lib_file
                Copy-Item -Path $lib_file_path -Destination $executable_directory_path
        }
}

"Finished"

Pop-Location