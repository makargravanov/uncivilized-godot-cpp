//
// Created by Alex on 27.06.2025.
//

#include "SystemNexus.h"

MapManager* SystemNexus::mapManager = nullptr;
std::unique_ptr<ClimateState> SystemNexus::climateState = nullptr;
std::future<ClimateState> SystemNexus::pendingClimateTurn;
bool SystemNexus::climateTurnInProgress = false;
u32 SystemNexus::appliedClimateBiomeYears = 0;
PlayScene* SystemNexus::play = nullptr;