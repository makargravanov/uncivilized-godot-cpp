//
// Created by Alex on 24.06.2025.
//


#include "LayerSeparator.h"

#include "DiscreteLandTypeByHeight.h"
#include <algorithm>
#include <cmath>
#include <queue>

#include "godot_cpp/classes/fast_noise_lite.hpp"

f32 quantile(std::vector<f32>& data, const f32 landPercentage) {
    std::sort(data.begin(), data.end());
    const f32 index    = landPercentage * (data.size() - 1);
    const f32 lower    = std::floor(index);
    const f32 fraction = index - lower;
    return data[lower] + fraction * (data[lower + 1] - data[lower]);
}

f32 LayerSeparator::findThreshold(
    const std::unique_ptr<f32[]>& heightMap, const f32 landPercentage, const u32 width, const u32 height) {

    auto vec = std::vector<f32>(width * height);
    for (u32 i = 0; i < width * height; ++i) {
        vec[i] = heightMap[i];
    }
    return quantile(vec, landPercentage);
}

void LayerSeparator::fillOceanOrValleyOrPlain(const std::unique_ptr<f32[]>& heights,
    std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete, const u32 width, const u32 height, const f32 oceanLevel) {

    for (u32 i = 0; i < width * height; ++i) {
        discrete[i] = PLAIN;
    }

    std::queue<std::pair<u32, u32>> toExpand;

    for (u32 x = 0; x < width; ++x) {
        if (heights[x] <= oceanLevel) {
            toExpand.emplace(x, 0);
            discrete[x] = OCEAN;
        }
        if (heights[(height - 1) * width + x] <= oceanLevel) {
            toExpand.emplace(x, height - 1);
            discrete[(height - 1) * width + x] = OCEAN;
        }
    }

    for (u32 y = 0; y < height; ++y) {
        if (heights[y * width] <= oceanLevel) {
            toExpand.emplace(0, y);
            discrete[y * width] = OCEAN;
        }
        if (heights[y * width + (width - 1)] <= oceanLevel) {
            toExpand.emplace(width - 1, y);
            discrete[y * width + (width - 1)] = OCEAN;
        }
    }

    while (!toExpand.empty()) {
        auto [x, y] = toExpand.front();
        toExpand.pop();

        for (i32 dy = -1; dy <= 1; ++dy) {
            for (i32 dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                i32 nx = static_cast<i32>(x) + dx;
                i32 ny = static_cast<i32>(y) + dy;
                if (nx >= 0 && nx < static_cast<i32>(width) && ny >= 0 && ny < static_cast<i32>(height)) {
                    u32 nidx = ny * width + nx;
                    if (discrete[nidx] == PLAIN && heights[nidx] <= oceanLevel) {
                        discrete[nidx] = OCEAN;
                        toExpand.emplace(static_cast<u32>(nx), static_cast<u32>(ny));
                    }
                }
            }
        }
    }

    for (u32 i = 0; i < width * height; ++i) {
        if (discrete[i] == PLAIN && heights[i] <= oceanLevel) {
            discrete[i] = VALLEY;
        }
    }
}

std::unique_ptr<u32[]> LayerSeparator::computeDistanceFromLand(
    const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width, const u32 height)
{
    constexpr u32 INFINITY_DIST = std::numeric_limits<u32>::max();

    auto distance = std::make_unique<u32[]>(width * height);

    // Инициализация: суша = 0, вода = бесконечность
    for (u32 i = 0; i < width * height; ++i) {
        if (discrete[i] == OCEAN || discrete[i] == VALLEY) {
            distance[i] = INFINITY_DIST;
        } else {
            distance[i] = 0;
        }
    }

    // BFS от всех клеток суши одновременно
    std::queue<std::pair<u32, u32>> queue;

    // Добавляем все клетки суши, граничащие с водой
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 idx = y * width + x;
            if (distance[idx] == 0) {
                // Проверяем, есть ли рядом вода
                bool nearWater = false;
                for (i32 dy = -1; dy <= 1 && !nearWater; ++dy) {
                    for (i32 dx = -1; dx <= 1 && !nearWater; ++dx) {
                        if (dx == 0 && dy == 0) continue;

                        const u32 nx = wrapX(static_cast<i32>(x) + dx, width);
                        const i32 nyRaw = static_cast<i32>(y) + dy;

                        if (nyRaw < 0 || nyRaw >= static_cast<i32>(height)) continue;

                        const u32 nidx = static_cast<u32>(nyRaw) * width + nx;
                        if (distance[nidx] == INFINITY_DIST) {
                            nearWater = true;
                        }
                    }
                }
                if (nearWater) {
                    queue.emplace(x, y);
                }
            }
        }
    }

    // BFS с 8-связностью
    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop();

        const u32 currentIdx = y * width + x;
        const u32 currentDist = distance[currentIdx];

        for (i32 dy = -1; dy <= 1; ++dy) {
            for (i32 dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;

                const u32 nx = wrapX(static_cast<i32>(x) + dx, width);
                const i32 nyRaw = static_cast<i32>(y) + dy;

                if (nyRaw < 0 || nyRaw >= static_cast<i32>(height)) continue;

                const u32 ny = static_cast<u32>(nyRaw);
                const u32 nidx = ny * width + nx;

                const u32 newDist = currentDist + 1;

                if (newDist < distance[nidx]) {
                    distance[nidx] = newDist;
                    queue.emplace(nx, ny);
                }
            }
        }
    }

    return distance;
}

void LayerSeparator::markCoastalShallows(
    std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const std::unique_ptr<u32[]>& distanceMap,
    const u32 width, const u32 height,
    const u64 seed)
{
    godot::Ref<godot::FastNoiseLite> noise;
    noise.instantiate();
    noise->set_noise_type(godot::FastNoiseLite::TYPE_SIMPLEX);
    noise->set_seed(static_cast<i32>(seed));
    noise->set_frequency(COASTAL_NOISE_SCALE);
    noise->set_fractal_type(godot::FastNoiseLite::FRACTAL_FBM);
    noise->set_fractal_octaves(3);

    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 idx = y * width + x;

            // Пропускаем сушу и уже обработанные клетки
            if (discrete[idx] != OCEAN && discrete[idx] != VALLEY) {
                continue;
            }

            const u32 dist = distanceMap[idx];

            // Если расстояние бесконечно (изолированная вода без суши рядом), пропускаем
            if (dist == std::numeric_limits<u32>::max()) {
                continue;
            }

            // Получаем значение шума в диапазоне [-1, 1]
            const f32 noiseValue = noise->get_noise_2d(
                static_cast<f32>(x),
                static_cast<f32>(y)
            );

            // Вычисляем порог с вариацией
            const f32 threshold = COASTAL_BASE_DISTANCE + noiseValue * COASTAL_VARIATION;

            // Если расстояние меньше или равно порогу — это мелководье
            if (static_cast<f32>(dist) <= threshold) {
                if (discrete[idx] == OCEAN) {
                    discrete[idx] = SHALLOW_OCEAN;
                } else if (discrete[idx] == VALLEY) {
                    discrete[idx] = SHALLOW_VALLEY;
                }
            }
        }
    }
}

void LayerSeparator::markHighWaterAsShallow(
    const std::unique_ptr<f32[]>& heights,
    std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width, const u32 height,
    const f32 percentile)
{
    // Собираем высоты глубокого океана и впадин (OCEAN и VALLEY, но не SHALLOW_*)
    std::vector<std::pair<f32, u32>> oceanHeights; // (высота, индекс)
    oceanHeights.reserve(width * height / 2);

    for (u32 i = 0; i < width * height; ++i) {
        if (discrete[i] == OCEAN) {
            oceanHeights.emplace_back(heights[i], i);
        }
    }

    if (oceanHeights.empty()) {
        return;
    }

    // Сортируем по высоте
    std::sort(oceanHeights.begin(), oceanHeights.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Находим порог для (1 - percentile), т.е. верхние 5%
    const f32 thresholdIndex = (1.0f - percentile) * static_cast<f32>(oceanHeights.size() - 1);
    const u32 thresholdIdx = static_cast<u32>(std::ceil(thresholdIndex));
    const f32 heightThreshold = oceanHeights[thresholdIdx].first;

    // Помечаем все клетки океана выше порога как мелководье
    for (u32 i = 0; i < width * height; ++i) {
        if (discrete[i] == OCEAN && heights[i] >= heightThreshold) {
            discrete[i] = SHALLOW_OCEAN;
        }
    }

    // Аналогично для VALLEY (внутренних морей)
    std::vector<std::pair<f32, u32>> valleyHeights;
    for (u32 i = 0; i < width * height; ++i) {
        if (discrete[i] == VALLEY) {
            valleyHeights.emplace_back(heights[i], i);
        }
    }

    if (!valleyHeights.empty()) {
        std::sort(valleyHeights.begin(), valleyHeights.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        const f32 valleyThresholdIndex = (1.0f - percentile) * static_cast<f32>(valleyHeights.size() - 1);
        const u32 valleyThresholdIdx = static_cast<u32>(std::ceil(valleyThresholdIndex));
        const f32 valleyHeightThreshold = valleyHeights[valleyThresholdIdx].first;

        for (u32 i = 0; i < width * height; ++i) {
            if (discrete[i] == VALLEY && heights[i] >= valleyHeightThreshold) {
                discrete[i] = SHALLOW_VALLEY;
            }
        }
    }
}

std::unique_ptr<f32[]> LayerSeparator::computeReliefMap(
    const std::unique_ptr<f32[]>& heights,
    const std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width, const u32 height, const u32 radius)
{
    auto prominence = std::make_unique<f32[]>(width * height);
    const i32 r = static_cast<i32>(radius);

    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 idx = y * width + x;

            // Пропускаем все водные типы
            if (discrete[idx] == OCEAN || discrete[idx] == VALLEY ||
                discrete[idx] == SHALLOW_OCEAN || discrete[idx] == SHALLOW_VALLEY) {
                prominence[idx] = 0.0f;
                continue;
            }

            f32 minH = std::numeric_limits<f32>::max();
            bool hasValidNeighbor = false;

            for (i32 dy = -r; dy <= r; ++dy) {
                for (i32 dx = -r; dx <= r; ++dx) {
                    if (dx == 0 && dy == 0) continue;

                    u32 nx = wrapX(static_cast<i32>(x) + dx, width);
                    i32 nyRaw = static_cast<i32>(y) + dy;

                    if (nyRaw < 0 || nyRaw >= static_cast<i32>(height)) {
                        continue;
                    }
                    u32 ny = static_cast<u32>(nyRaw);

                    u32 nidx = ny * width + nx;
                    if (discrete[nidx] != OCEAN && discrete[nidx] != VALLEY &&
                        discrete[nidx] != SHALLOW_OCEAN && discrete[nidx] != SHALLOW_VALLEY) {
                        minH = std::min(minH, heights[nidx]);
                        hasValidNeighbor = true;
                    }
                }
            }

            if (hasValidNeighbor) {
                prominence[idx] = std::max(0.0f, heights[idx] - minH);
            } else {
                prominence[idx] = 0.0f;
            }
        }
    }

    return prominence;
}

void LayerSeparator::normalizeMap(f32* map, const u32 size) {
    f32 minVal = std::numeric_limits<f32>::max();
    f32 maxVal = std::numeric_limits<f32>::lowest();

    for (u32 i = 0; i < size; ++i) {
        minVal = std::min(minVal, map[i]);
        maxVal = std::max(maxVal, map[i]);
    }

    const f32 range = (maxVal - minVal > 1e-6f) ? (maxVal - minVal) : 1.0f;

    for (u32 i = 0; i < size; ++i) {
        map[i] = (map[i] - minVal) / range;
    }
}

SeparatedMapResult LayerSeparator::initializeOceanAndThresholdsByGradient(MapResult&& map) {
    const u32 width  = map.width;
    const u32 height = map.height;
    const u32 size   = width * height;

    auto heights  = std::move(map.heights);
    auto discrete = std::make_unique<DiscreteLandTypeByHeight[]>(size);

    // 1. Заполняем OCEAN, VALLEY, PLAIN
    fillOceanOrValleyOrPlain(heights, discrete, width, height, map.oceanLevel);

    // 2. Вычисляем расстояние от суши
    auto distanceMap = computeDistanceFromLand(discrete, width, height);

    // 3. Помечаем прибрежное мелководье с шумом
    markCoastalShallows(discrete, distanceMap, width, height, map.seed);

    // 4. Помечаем высокие точки океана как мелководье
    markHighWaterAsShallow(heights, discrete, width, height, SHALLOW_OCEAN_PERCENTILE);

    // 5. Вычисляем рельеф и определяем холмы/горы
    auto reliefMap = computeReliefMap(heights, discrete, width, height, RELIEF_WINDOW_RADIUS);
    auto normHeight = std::make_unique<f32[]>(size);
    auto normRelief = std::make_unique<f32[]>(size);

    for (u32 i = 0; i < size; ++i) {
        normHeight[i] = heights[i];
        normRelief[i] = reliefMap[i];
    }

    normalizeMap(normHeight.get(), size);
    normalizeMap(normRelief.get(), size);

    auto score = std::make_unique<f32[]>(size);
    std::vector<f32> landScores;
    landScores.reserve(size / 2);

    for (u32 i = 0; i < size; ++i) {
        if (discrete[i] == PLAIN) {
            score[i] = HEIGHT_WEIGHT * normHeight[i] + (1.0f - HEIGHT_WEIGHT) * normRelief[i];
            landScores.push_back(score[i]);
        } else {
            score[i] = 0.0f;
        }
    }

    f32 thresholdHill     = 0.0f;
    f32 thresholdMountain = 0.0f;

    if (!landScores.empty()) {
        std::sort(landScores.begin(), landScores.end());

        auto getPercentile = [&](f32 p) -> f32 {
            f32 idx  = p * static_cast<f32>(landScores.size() - 1);
            u32 lo   = static_cast<u32>(std::floor(idx));
            f32 frac = idx - static_cast<f32>(lo);
            if (lo + 1 < landScores.size()) {
                return landScores[lo] + frac * (landScores[lo + 1] - landScores[lo]);
            }
            return landScores[lo];
        };

        thresholdHill     = getPercentile(HILL_PERCENTILE);
        thresholdMountain = getPercentile(MOUNTAIN_PERCENTILE);
    }

    for (u32 i = 0; i < size; ++i) {
        if (discrete[i] != PLAIN) {
            continue;
        }

        if (score[i] >= thresholdMountain) {
            if (normRelief[i] < PLATEAU_RELIEF_THRESHOLD) {
                discrete[i] = PLAIN; // TODO: плато сделать возможно?
            } else {
                discrete[i] = MOUNTAIN;
            }
        } else if (score[i] >= thresholdHill) {
            discrete[i] = HILL;
        }
    }

    auto mapResult =
        MapResult(map.seed, std::move(heights), std::move(map.ageMap), std::move(map.platesMap), width, height, map.oceanLevel);

    return {std::move(mapResult), std::move(discrete), map.oceanLevel, thresholdHill, thresholdMountain};
}

SeparatedMapResult LayerSeparator::initializeOceanAndThresholdsByGradient(MapResult&& map, f32 oceanLevelOverride) {
    map.oceanLevel = oceanLevelOverride;
    return initializeOceanAndThresholdsByGradient(std::move(map));
}
// TODO:
//  - Организовать выравнивание высот относительно уровня океана
//  - Отдельно сохранять рельеф дна
