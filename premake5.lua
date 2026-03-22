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

        filter "platforms:Windows"
                defines { "OS_WINDOWS" }
                system ("windows")

        filter "platforms:Linux"
                defines { "OS_LINUX" }
                system ("linux")

        filter "configurations:Debug"
                links { "$(VULKAN_SDK)/lib/vulkan-1.lib", "External/KTX/lib/ktx.lib", "$(VULKAN_SDK)/lib/SDL3d.lib", "$(VULKAN_SDK)/lib/volkd.lib", "$(VULKAN_SDK)/lib/slangd.lib"}
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"

        filter "configurations:Development"
                links { "$(VULKAN_SDK)/lib/vulkan-1.lib", "External/KTX/lib/ktx.lib", "$(VULKAN_SDK)/lib/SDL3d.lib", "$(VULKAN_SDK)/lib/volkd.lib", "$(VULKAN_SDK)/lib/slangd.lib"}
                defines { "DEBUG", "DEBUG_TRACE", "DEBUG_INFO", "DEBUG_WARNINGS"}
                symbols "On"
                optimize "Debug"

        filter "configurations:Release"
              -- kind "WindowedApp"

                links { "$(VULKAN_SDK)/lib/vulkan-1.lib", "External/KTX/lib/ktx.lib", "$(VULKAN_SDK)/lib/SDL3.lib", "$(VULKAN_SDK)/lib/volk.lib", "$(VULKAN_SDK)/lib/slang.lib"}
                defines { "RELEASE" }
                optimize "On"