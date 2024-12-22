-- xmake.lua 用以生成 xmake cache 相关目录

target(path.basename(os.curdir()) .. ".elf")
set_targetdir("Debug")
