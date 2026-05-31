workspace "SpaceGame"
        configurations { "Debug", "Development", "Release" }
        platforms { "Windows", "Linux" }
        architecture "x64"

project "SpaceGame"
        -- Language setup --
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++23"

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
                "$(VULKAN_SDK)/include",
        }

        libdirs {
                "$(VULKAN_SDK)/lib/",
        }

        links {
                "vulkan",
                "SDL3",
                "External/KTX/lib/ktx",
                "volk",
                "slang",
        }

        -- Settings --
        externalwarnings "Off"
        defines { "VK_NO_PROTOTYPES" }

        -- externalincludedirs { "External/", "External/KTX", "$(VULKAN_SDK)/include" }
        -- warnings "Extra"
        -- enableunitybuild "On"


        -- links { "$(VULKAN_SDK)/lib/vulkan", "External/KTX/lib/ktx", "$(VULKAN_SDK)/lib/SDL3", "$(VULKAN_SDK)/lib/volk", "$(VULKAN_SDK)/lib/slang"}

        -- filter "platforms:Windows"
        --         defines { "OS_WINDOWS" }
        --         system ("windows")

        -- filter "platforms:Linux"
        --         defines { "OS_LINUX" }
        --         system ("linux")
        --         disablewarnings { "missing-field-initializers" }

        -- filter "configurations:Debug"
        --         defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
        --         symbols "On"

        -- filter "configurations:Development"
        --         defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
        --         symbols "On"
        --         optimize "Debug"

        -- filter "configurations:Release"
        --       -- kind "WindowedApp"
        --         defines { "RELEASE" }
        --         optimize "On"

project "ShaderCheck"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++23"
        location "Tools/Build"

        files { "Tools/ShaderCheck.cpp" }

        targetdir "Tools/Bin/%{cfg.buildcfg}"

        includedirs { "Src/" }
        externalincludedirs { "$(VULKAN_SDK)/include" }

        warnings "Extra"

        enableunitybuild "On"
        externalwarnings "Off"

        links { "$(VULKAN_SDK)/lib/slang" }