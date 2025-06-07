#!/usr/bin/env python
import os
import sys

from methods import print_error

libname = "EXTENSION-NAME"
projectdir = "demo"

localEnv = Environment(tools=["default"], PLATFORM="")

# Добавляем новые переменные для управления сборкой
opts = Variables(["custom.py"], ARGUMENTS)
opts.Add(EnumVariable("build_type", "Тип сборки (debug/release/native/aggressive)", "debug",
                      ("debug", "release", "native", "aggressive")))
opts.Add(BoolVariable("ext_numeric", "Включить поддержку числовых литералов", True))
opts.Add(BoolVariable("use_mingw", "Использовать MinGW", False))
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

# Проверка подмодулей
if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""Подмодуль godot-cpp отсутствует в этой папке, так как подмодули Git не были инициализированы.
Выполните следующую команду для загрузки godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": ["custom.py"]})

# Получаем значения переменных из окружения
ext_numeric = env.get('ext_numeric', True)
platform = env.get('platform', '')
use_mingw = env.get('use_mingw', False)
build_type = env.get('build_type', 'debug')

# Добавляем флаг для числовых литералов если нужно
if ext_numeric and platform == "windows" and use_mingw:
    env.Append(CXXFLAGS=["-fext-numeric-literals"])
    print("Включен флаг -fext-numeric-literals для MinGW")

print(f"Выбран тип сборки: {build_type}")

# Настройки для разных типов сборки
if build_type == "release":
    # Стандартные оптимизации для релиза
    env.Append(CPPDEFINES=["NDEBUG"])
    env.Append(CXXFLAGS=["-O3", "-flto"])
    print("Применены настройки стандартной релизной сборки: -O3, -flto")

elif build_type == "native":
    # Оптимизация под текущий процессор
    env.Append(CPPDEFINES=["NDEBUG"])
    env.Append(CXXFLAGS=["-O3", "-flto", "-march=native", "-mtune=native"])
    print("Применены настройки нативной сборки: -O3, -flto, -march=native, -mtune=native")

elif build_type == "aggressive":
    # Агрессивные и потенциально небезопасные оптимизации
    env.Append(CPPDEFINES=["NDEBUG"])
    env.Append(CXXFLAGS=[
        "-O3",
        "-flto",
        "-march=native",
        "-mtune=native",
        "-ffast-math",
        "-funroll-loops",
        "-fomit-frame-pointer",
        "-fno-signed-zeros",
        "-fno-trapping-math",
        "-fassociative-math",
        "-fno-stack-protector",
        "-fdevirtualize-at-ltrans",
        "-fipa-pta",
        "-fno-semantic-interposition",
        "-fno-plt"
    ])
    print("ВНИМАНИЕ: Применены агрессивные оптимизации, которые могут влиять на точность вычислений!")
    print("Флаги оптимизации: -O3, -flto, -march=native, -ffast-math, -funroll-loops, -fomit-frame-pointer")
    print("и другие агрессивные оптимизации для максимальной производительности")

else:  # debug (по умолчанию)
    env.Append(CPPDEFINES=["DEBUG"])
    env.Append(CXXFLAGS=["-g", "-O0"])
    print("Применены настройки отладочной сборки: -g, -O0")

env.Append(CPPPATH=["src/"])
env.Append(CPPPATH=["outer-libs/"])

# Сборка исходников
sources = Glob("src/*.cpp")
outer_libs_sources = Glob("outer-libs/**/*.cpp")
sources.extend(outer_libs_sources)

# Генерация документации (если нужно)
target = env.get('target', '')
if target in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Документация классов не включена")

# Формирование имени библиотеки
suffix = env.get('suffix', '').replace(".dev", "").replace(".universal", "")
suffix += f".{build_type}"  # Добавляем тип сборки в имя файла

lib_filename = f"{env.subst('$SHLIBPREFIX')}{libname}{suffix}{env.subst('$SHLIBSUFFIX')}"

# Сборка и установка
library = env.SharedLibrary(
    f"bin/{platform}/{lib_filename}",
    source=sources,
)

copy = env.Install(f"{projectdir}/bin/{platform}/", library)

# Целевые объекты по умолчанию
Default(library, copy)