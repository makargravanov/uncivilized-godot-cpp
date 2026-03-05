//
// Created by Copilot on 06.03.2026.
//

#ifndef VIEWMODE_H
#define VIEWMODE_H

#include "util/declarations.h"

// Visual overlay modes for map inspection.
// Matches the uniform int view_mode in tile shaders.
enum ViewMode : u8 {
    VIEW_NORMAL      = 0,  // Default biome/terrain rendering
    VIEW_TEMPERATURE = 1,  // Blue (cold) → Red (hot)
    VIEW_MOISTURE    = 2,  // Brown (dry) → Blue (wet)
    VIEW_ELEVATION   = 3,  // Green (low) → White (high)
    VIEW_BIOME       = 4,  // Distinct colour per biome ID
    VIEW_MODE_COUNT
};

#endif //VIEWMODE_H
