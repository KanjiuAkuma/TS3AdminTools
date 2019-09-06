build_dir = "build\\"
bin_dir = build_dir .. "%{prj.name}\\bin\\%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
bin_int_dir = build_dir .. "%{prj.name}\\bin-int\\%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

ts_plugin_dir = "C:/Users/Kanjiu Akuma/AppData/Roaming/TS3Client/plugins"

workspace "TS3AdminTools"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
	}

project "TS3AdminTools"
	location "TS3AdminTools"
	language "C++"
	cppdialect "C++17"
	systemversion "latest"
	kind "SharedLib"

	targetdir("%{wks.location}/" .. bin_dir)
	objdir("%{wks.location}/" .. bin_int_dir)

	files {
		"%{prj.name}/src/**",
	}

	includedirs {
		"%{prj.name}/src",
		"%{prj.name}/src/vendor",
	}

	postbuildcommands {
		"copy /Y \"%{wks.location}" .. bin_dir .. "\\%{prj.name}.dll\" \"" .. ts_plugin_dir .. "/\""
	}

	filter "configurations:Debug"
		symbols "Full"
		defines {
			"AT_DEBUG"
		}

	filter "configurations:Release"
		optimize "On"
		defines {
			"AT_RELEASE"
		}