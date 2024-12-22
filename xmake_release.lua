
target(path.basename(os.curdir()) .. ".elf")
set_targetdir("Release")

set_kind("binary")

add_files("src/**.c")
add_files("src/**.cc")
add_files("src/**.cpp")

add_includedirs("src")
add_includedirs("src/lib")
add_includedirs(".")

add_defines("_GNU_SOURCE")

add_cflags("-O2")
add_cflags("-g0")
add_cflags("-Wall")
add_cflags("-U_FORTIFY_SOURCE")
add_cflags("-D_FORTIFY_SOURCE=0")

add_cxxflags("-O2")
add_cxxflags("-g0")
add_cxxflags("-Wall")
add_cxxflags("-U_FORTIFY_SOURCE")
add_cxxflags("-D_FORTIFY_SOURCE=0")

add_links("pthread")
add_links("fftw3f")
add_links("dl")

-- 添加链接库搜索路径
add_linkdirs("library")

-- add_ldflags("")

-- set_languages("gun99", "gnu++14")

on_load(function (target)
    target_name = target:name()
    print(target_name)

    -- target:add("files", "main.cc")
end)
