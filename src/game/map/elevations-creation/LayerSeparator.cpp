//
// Created by Alex on 24.06.2025.
//

#include "LayerSeparator.h"

#include "DiscreteLandTypeByHeight.h"
#include <algorithm>
#include <cmath>
#include <queue>

#include "godot_cpp/classes/fast_noise_lite.hpp"
#include "godot_cpp/classes/ref.hpp"

f32 quantile(std::vector<f32>& data, const f32 landPercentage) {
    std::sort(data.begin(), data.end());
    const f32 index    = landPercentage * (data.size() - 1);
    const f32 lower    = std::floor(index);
    const f32 fraction = index - lower;
    return data[lower] + fraction * (data[lower+1] - data[lower]);
}

f32 LayerSeparator::findThreshold(           // от 0.0 до 1.0
    const std::unique_ptr<f32[]>& heightMap, const f32 landPercentage, const u32 width, const u32 height) {

    auto vec = std::vector<f32>(width*height);
    for (int i = 0; i < width*height; ++i) {
        vec[i] = heightMap[i];
    }
    return quantile(vec, landPercentage);
}

void LayerSeparator::fillOceanOrValleyOrPlain(const std::unique_ptr<f32[]>& heights, std::unique_ptr<DiscreteLandTypeByHeight[]>& discrete,
    const u32 width, const u32 height, const f32 oceanLevel) {

    for (u32 i = 0; i < width * height; ++i) {
        discrete[i] = PLAIN;
    }

    std::queue<std::pair<u32, u32>> toExpand;

    for (u32 x = 0; x < width; ++x) {
        if (heights[x] <= oceanLevel) {
            toExpand.push({x, 0});
            discrete[x] = OCEAN;
        }
        if (heights[(height - 1) * width + x] <= oceanLevel) {
            toExpand.push({x, height - 1});
            discrete[(height - 1) * width + x] = OCEAN;
        }
    }

    for (u32 y = 0; y < height; ++y) {
        if (heights[y * width] <= oceanLevel) {
            toExpand.push({0, y});
            discrete[y * width] = OCEAN;
        }
        if (heights[y * width + (width - 1)] <= oceanLevel) {
            toExpand.push({width - 1, y});
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
                        toExpand.push({static_cast<u32>(nx), static_cast<u32>(ny)});
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
SeparatedMapResult LayerSeparator::initializeOceanAndThresholds(MapResult&& map)
{
    // 1. Переносим данные из входного map
    auto heights  = std::move(map.heights);
    auto discrete = std::make_unique<DiscreteLandTypeByHeight[]>(map.height * map.width);

    // 2. Заполняем океан / долины / равнины (твоя функция)
    fillOceanOrValleyOrPlain(heights, discrete, map.width, map.height, map.oceanLevel);

    // 3. Находим пороги для холмов и гор (перцентили)
    f32 thresholdHill     = findThreshold(heights, 0.90f, map.width, map.height);
    f32 thresholdMountain = findThreshold(heights, 0.97f, map.width, map.height);

    // 4. Базовая классификация по высоте (только равнины → холмы / горы)
    u32 size = map.width * map.height;
    for (u32 i = 0; i < size; ++i)
    {
        if (discrete[i] == PLAIN)
        {
            if (heights[i] >= thresholdHill && heights[i] < thresholdMountain)
            {
                discrete[i] = HILL;
            }
            else if (heights[i] >= thresholdMountain)
            {
                discrete[i] = MOUNTAIN;
            }
        }
    }

    // ────────────────────────────────────────────────────────────────
    // 5. Добавляем вариативность с помощью FastNoiseLite (Godot 4)
    // ────────────────────────────────────────────────────────────────

    godot::Ref<godot::FastNoiseLite> noise;
    noise.instantiate();

    // Настройки шума — подбирай под свою карту
    noise->set_seed(23);                               // можно брать из Platec seed
    noise->set_noise_type(godot::FastNoiseLite::TYPE_SIMPLEX_SMOOTH);  // или TYPE_SIMPLEX
    noise->set_frequency(0.1f);                      // 0.02 – 0.08 обычно хорошо для 400×250, чем выше тем мельче пятна
    noise->set_fractal_type(godot::FastNoiseLite::FRACTAL_FBM);
    noise->set_fractal_octaves(10);                     // 4–6 обычно достаточно
    noise->set_fractal_gain(0.5f);
    noise->set_fractal_lacunarity(2.6f);

    /**
     *fractal_lacunarity -> чем больше тем более зернистый шум
     *fractal_gain -> чем больше тем более зернистый шум и опять же тем больше мелких деталей
     */

    noise->set_domain_warp_enabled(true);
    noise->set_domain_warp_type(godot::FastNoiseLite::DOMAIN_WARP_SIMPLEX);  // самый популярный и красивый
    noise->set_domain_warp_amplitude(30.0f); // сила искажения — от 10 до 100 обычно

    // Пороги для понижения типа (шум в диапазоне [-1, +1])
    constexpr float NOISE_THRESHOLD_MOUNTAIN_TO_HILL = 0.15f;   // чем ниже — тем больше гор → холмы
    constexpr float NOISE_THRESHOLD_HILL_TO_PLAIN    = 0.75f;   // чем ниже — тем больше холмов → равнины

    // Применяем шум только к холмам и горам
    for (u32 y = 0; y < map.height; ++y)
    {
        for (u32 x = 0; x < map.width; ++x)
        {
            u32 idx = y * map.width + x;

            // Получаем значение шума в точке
            float n = noise->get_noise_2d(static_cast<float>(x), static_cast<float>(y));

            // Можно дополнительно умножить на амплитуду или добавить offset
            // n = n * 0.8f + 0.1f;  // пример смещения, если нужно

            switch (discrete[idx])
            {
                case MOUNTAIN:
                    if (n > NOISE_THRESHOLD_MOUNTAIN_TO_HILL)
                    {
                        discrete[idx] = HILL;
                    }
                    break;

                case HILL:
                    if (n > NOISE_THRESHOLD_HILL_TO_PLAIN)
                    {
                        discrete[idx] = PLAIN;
                    }
                    break;

                default:
                    // Океан, равнина и т.д. не трогаем
                    break;
            }
        }
    }

    // 6. Собираем результат
    auto mapResult = MapResult(
        std::move(heights),
        std::move(map.ageMap),
        std::move(map.platesMap),
        map.width,
        map.height,
        map.oceanLevel
    );

    return SeparatedMapResult(
        std::move(mapResult),
        std::move(discrete),
        map.oceanLevel,
        thresholdHill,
        thresholdMountain
    );
}
SeparatedMapResult LayerSeparator::initializeOceanAndThresholds(MapResult&& map, f32 oceanLevelOverride) {
    map.oceanLevel = oceanLevelOverride;
    return initializeOceanAndThresholds(std::move(map));
}

//TODO:
// - Организовать выравнивание высот относительно уровня океана
// - Отдельно сохранять рельеф дна