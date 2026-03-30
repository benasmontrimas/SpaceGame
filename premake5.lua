workspace "SpaceGame"
        configurations { "Debug", "Development", "Release" }
        platforms { "Windows", "Linux" }
        architecture "x64"

project "SpaceGame"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++23"
        location "Build/"

        files { "Src/**.h", "Src/**.cpp" }

        targetdir "Bin/%{cfg.buildcfg}"

        includedirs { "Src/" }
        externalincludedirs { "External/", "External/KTX", "$(VULKAN_SDK)/include" }

        warnings "Extra"

        enableunitybuild "On"
        externalwarnings "Off"

        defines { "VK_NO_PROTOTYPES" }

        links { "$(VULKAN_SDK)/lib/vulkan", "External/KTX/lib/ktx", "$(VULKAN_SDK)/lib/SDL3", "$(VULKAN_SDK)/lib/volk", "$(VULKAN_SDK)/lib/slang"}

        filter "platforms:Windows"
                defines { "OS_WINDOWS" }
                system ("windows")

        filter "platforms:Linux"
                defines { "OS_LINUX" }
                system ("linux")
                disablewarnings { "missing-field-initializers" }

        filter "configurations:Debug"
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"

        filter "configurations:Development"
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"
                optimize "Debug"

        filter "configurations:Release"
              -- kind "WindowedApp"
                defines { "RELEASE" }
                optimize "On"