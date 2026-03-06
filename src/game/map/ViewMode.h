//
// Created by Copilot on 06.03.2026.
//

#ifndef VIEWMODE_H
#define VIEWMODE_H

#include "util/declarations.h"

// Visual map inspection modes.
// Tile shaders currently use 0..4; higher values can be handled by separate debug geometry.
enum ViewMode : u8 {
    VIEW_NORMAL      = 0,  // Default biome/terrain rendering
    VIEW_TEMPERATURE = 1,  // Blue (cold) → Red (hot)
    VIEW_MOISTURE    = 2,  // Brown (dry) → Blue (wet)
    VIEW_ELEVATION   = 3,  // Green (low) → White (high)
    VIEW_BIOME       = 4,  // Distinct colour per biome ID
    VIEW_WIND        = 5,  // Debug wind vectors
    VIEW_MODE_COUNT
};

#endif //VIEWMODE_H
