add_rules("mode.debug", "mode.release")
set_policy("package.requires_lock", true)

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    set_description("Preloader Android")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

package("boost_pfr")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/boostorg/pfr")
    add_urls("https://github.com/boostorg/pfr.git")
    add_versions("boost-1.88.0", "boost-1.88.0")
    on_install(function (package)
        os.cp("include", package:installdir())
    end)
package_end()

package("magic_enum")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/Neargye/magic_enum")
    add_urls("https://github.com/Neargye/magic_enum.git")
    add_versions("v0.9.7", "v0.9.7")
    on_install(function (package)
        os.cp("include", package:installdir())
    end)
package_end()

add_requires("preloader")
add_requires("nlohmann_json v3.11.3")
add_requires("fmt")
add_requires("boost_pfr")
add_requires("magic_enum")

target("CustomSkies")
    set_kind("shared")
    set_languages("c++20")
    set_strip("all")
    add_files("src/**.cpp")
    add_includedirs("include", "src", {public = true})
    add_packages("preloader", "nlohmann_json", "fmt", "boost_pfr", "magic_enum")

    if is_plat("android") then
        add_cxflags("-fPIC", "-O2", "-ffunction-sections", "-fdata-sections", "-fexceptions", "-w")
        add_shflags("-Wl,--gc-sections", "-Wl,--hash-style=gnu", "-Wl,-z,max-page-size=16384")
        add_links("android", "log", "mediandk", "GLESv2", "EGL")
        add_shflags("-Wl,--unresolved-symbols=ignore-all")
    end

    after_build(function (target)
        if not target:is_plat("android") then return end
        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        assert(python, "Python 3 is required to package .levipack")
        local outdir = target:targetdir()
        local so_path = path.join(outdir, target:filename())
        local pack_path = path.join(outdir, "CustomSkies.levipack")
        os.execv(python.program, {
            "scripts/package_levipack.py",
            "--library", so_path,
            "--manifest", "manifest.json",
            "--config", "config/config.json",
            "--assets", "assets",
            "--output", pack_path
        })
        print("Packed: " .. pack_path)
    end)
