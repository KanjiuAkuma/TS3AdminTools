build_dir = "build\\"
bin_dir = build_dir .. "%{prj.name}\\bin\\%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
bin_int_dir = build_dir .. "%{prj.name}\\bin-int\\%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

ts_plugin_dir = "C:/Users/Kanjiu Akuma/AppData/Roaming/TS3Client/plugins"

workspace "Ts3AdminTools"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
	}

project "Ts3AdminTools"
	location "Ts3AdminTools"
	language "C++"
	cppdialect "C++17"
	systemversion "latest"

	targetdir("%{wks.location}/" .. bin_dir)
	objdir("%{wks.location}/" .. bin_int_dir)

	includedirs {
		"%{prj.name}/src",
		"%{prj.name}/src/vendor",
	}

	files {
		"%{prj.name}/src/**",
	}

	postbuildcommands {
		"copy /Y \"%{wks.location}" .. bin_dir .. "\\Ts3AdminTools.dll\" \"" .. ts_plugin_dir .. "/\""
	}

	filter "configurations:Debug"
		symbols "Full"
		kind "SharedLib"
		defines {
			"AT_DEBUG"
		}

	filter "configurations:Release"
		kind "SharedLib"
		optimize "On"
		defines {
			"AT_RELEASE"
		}