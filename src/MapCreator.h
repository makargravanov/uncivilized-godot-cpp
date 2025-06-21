//
// Created by Alex on 20.06.2025.
//

#ifndef MAPCREATOR_H
#define MAPCREATOR_H
#include <future>

#include <godot_cpp/classes/ref_counted.hpp>
#include "map/PlatecWrapper.h"

class MapCreator : public godot::RefCounted{
    GDCLASS(MapCreator, RefCounted)

protected:
    static void _bind_methods();

    std::future<void> futureResult;
    godot::Callable callbackHeightMap;
    godot::Callable callbackFinish;

private:
    void createInternal();
    bool allCallbacksValid() const;

public:
    MapCreator() = default;
    ~MapCreator() override;

    void set_heightmap_callback(const godot::Callable &callback);
    void set_finish_callback(const godot::Callable &callback);

    void create();
};



#endif //MAPCREATOR_H
