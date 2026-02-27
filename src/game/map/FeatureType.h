//
// Created by Alex on 27.02.2026.
//

#ifndef FEATURETYPE_H
#define FEATURETYPE_H

#include "util/declarations.h"

// Tile features / decorations — future layer (forests, rivers, swamps, etc.).
enum FeatureType : u8 {
    FEATURE_NONE = 0,
    // Future values:
    // FEATURE_FOREST_DECIDUOUS,
    // FEATURE_FOREST_CONIFER,
    // FEATURE_RIVER,
    // FEATURE_SWAMP,
};

#endif //FEATURETYPE_H
