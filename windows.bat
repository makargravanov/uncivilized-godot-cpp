cd godot-cpp
scons platform=windows use_mingw=yes target=template_release
cd ..
scons platform=windows use_mingw=yes target=template_release
scons compiledb=yes platform=windows use_mingw=yes
echo "ready"