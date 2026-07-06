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
        runtime "Release"

        -- Output Location --
        location "Build/"
        targetdir "Bin/%{cfg.buildcfg}"

        -- Input Locations --
        files { "Src/**.h", "Src/**.cpp" }
        includedirs { "Src/" }

        -- External Libraries --
        externalincludedirs {
                "External",
                "External/KTX",
                "External/FreeType",
                "External/fmod/inc/",
                "$(VULKAN_SDK)/include",
        }

        libdirs {
                "$(VULKAN_SDK)/lib/",
                "External/FreeType/lib/",
                "External/KTX/lib/",
                "External/fmod/lib/x64/",
        }

        runpathdirs {
                -- "External/slang/lib",
                -- "External/fmod/lib/x86_64/",
                -- "External/fmod/lib/x64/",
        }

        -- Settings --
        externalwarnings "Off"
        defines { "VK_NO_PROTOTYPES" }

        -- externalincludedirs { "External/", "External/KTX", "$(VULKAN_SDK)/include" }
        -- warnings "Extra"
        -- enableunitybuild "On"


        -- links { "$(VULKAN_SDK)/lib/vulkan", "External/KTX/lib/ktx", "$(VULKAN_SDK)/lib/SDL3", "$(VULKAN_SDK)/lib/volk", "$(VULKAN_SDK)/lib/slang"}

        filter "platforms:Windows"
                defines { "OS_WINDOWS" }
                system ("windows")

                links {
                        "vulkan-1",
                        "SDL3",
                        "External/KTX/lib/ktx",
                        "volk",
                        "slang",
                        "External/FreeType/lib/freetype",
                        "fmod_vc"
                }

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
                defines { "DEBUG", "_DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"

        filter "configurations:Development"
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"
                optimize "Debug"

        filter "configurations:Release"
              -- kind "WindowedApp"
                symbols "Off"
                defines { "RELEASE" }
                optimize "On"

-- project "ShaderCheck"
--         kind "ConsoleApp"
--         language "C++"
--         cppdialect "C++23"
--         location "Tools/Build"

--         files { "Tools/ShaderCheck.cpp" }

--         targetdir "Tools/Bin/%{cfg.buildcfg}"

--         includedirs { "Src/" }
--         externalincludedirs { "$(VULKAN_SDK)/include" }

--         warnings "Extra"

--         enableunitybuild "On"
--         externalwarnings "Off"

--         links { "$(VULKAN_SDK)/lib/slang" }