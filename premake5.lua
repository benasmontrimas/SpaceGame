workspace "SpaceGame"
        configurations { "Debug", "Development", "Release" }
        platforms { "Windows", "Linux" }
        architecture "x64"

project "SpaceGame"
        targetname "SpaceGame"
        -- Language setup --
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++23"
        symbols "On"
        staticruntime "off"

        -- Output Location --
        location "Build/"
        targetdir "Bin/%{cfg.buildcfg}"
        debugdir "Bin/%{cfg.buildcfg}"

        -- Input Locations --
        files { "Src/**.h", "Src/**.cpp" }
        includedirs { "Src/" }

        -- External Libraries --
        externalincludedirs {
                "External/include",
                "External/include/KTX",
                "External/include/FreeType",
                "External/include/fmod/inc/",
        }

        libdirs {
                "External/lib"
        }

        runpathdirs {
                -- "External/slang/lib",
                -- "External/fmod/lib/x86_64/",
                -- "External/fmod/lib/x64/",
        }

        -- Settings --
        externalwarnings "Off"
        defines { "VK_NO_PROTOTYPES" }

        filter "platforms:Windows"
                defines { "OS_WINDOWS" }
                system ("windows")

                links {
                        "vulkan-1",
                        "SDL3",
                        "ktx",
                        "volk",
                        "slang",
                        "freetype",
                        "fmod_vc"
                }

                -- filter "configurations:Release"
                kind "WindowedApp"
                postbuildcommands { "powershell -ExecutionPolicy Bypass -File ../Scripts/CopyAssets.ps1 %{cfg.buildcfg}" }
                -- postbuildcommands { "powershell -command Get-Location" }

        filter "platforms:Linux"
                defines { "OS_LINUX" }
                system ("linux")
                disablewarnings { "missing-field-initializers" }

                links {
                        "vulkan",
                        "SDL3",
                        "External/KTX/lib/ktx",
                        "volk",
                        "External/slang/lib/slang",
                        "freetype",
                        "External/fmod/lib/x86_64/fmod"
                }

        filter "configurations:Debug"
                kind "ConsoleApp"
                runtime "Debug"
                defines { "DEBUG", "_DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"

        filter "configurations:Development"
                runtime "Release"
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"
                optimize "Debug"

        filter "configurations:Release"
                runtime "Release"
                symbols "Off"
                defines { "RELEASE" }
                optimize "On"