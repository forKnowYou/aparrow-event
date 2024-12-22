
-- 设置目标文件名和生成路径
target(path.basename(os.curdir()) .. ".elf")
set_targetdir("Debug")
set_objectdir("~/work/" .. path.basename(os.curdir()) .. "/Debug")
set_dependir("~/work/" .. path.basename(os.curdir()) .. "/Debug")

-- 设置目标类型 -- binary | static | shared
set_kind("binary")

-- 添加要编译的文件
add_files("**.c")
add_files("**.cc")
add_files("**.cpp")

add_files("src/context.S")

-- 添加要编译的文件同时排除路径或文件
-- 递归添加 src 目录下的所有 C 文件但是排除 xxx 路径
-- add_files("src/**.c|xxx/**.c")
-- 递归添加 src 目录下的所有 C 文件但是排除 xxx 文件
-- add_files("src/**.c|xxx.c")

-- 按顺序添加头文件搜索路径
add_includedirs("inc")
add_includedirs(".")

-- 添加全局宏定义
-- add_defines("_GNU_SOURCE")

-- 添加 C/C++ 文件的编译选项
add_cxflags("-O0")
add_cxflags("-g3")
add_cxflags("-Wall")
-- add_cflags("-U_FORTIFY_SOURCE")
-- add_cflags("-D_FORTIFY_SOURCE=0")

-- 添加链接库

-- add_links("dl")
-- add_links("rt")
add_links("pthread")

-- 添加链接库搜索路径
-- add_linkdirs("lib")

-- 添加链接选项
-- add_ldflags("-fstack-protector-all")
-- add_ldflags("-static")

-- 设置语言标准
-- set_languages("gun99", "gnu++17")

-- 在启动时使用 lua 脚本进行复杂配置
on_load(function (target)
    target_name = target:name()
    print(target_name)

    -- target:add("files", "main.cc")
end)
