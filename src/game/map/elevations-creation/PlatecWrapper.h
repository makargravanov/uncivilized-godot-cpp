//
// Created by Alex on 11.06.2025.
//

#ifndef GODOT_CPP_TEMPLATE_MAPCREATOR_H
#define GODOT_CPP_TEMPLATE_MAPCREATOR_H

#include "../../../util/declarations.h"
#include <future>

struct MapResult {
    std::unique_ptr<f32[]> heights;
    std::unique_ptr<u32[]> ageMap;
    std::unique_ptr<u16[]> platesMap;
    u32 width  = 0;
    u32 height = 0;
    f32 oceanLevel = 0;

    MapResult(std::unique_ptr<f32[]> heights, std::unique_ptr<u32[]> ageMap,std::unique_ptr<u16[]> platesMap, u32 width, u32 height, f32 oceanLevel)
        : heights(std::move(heights)),
          ageMap(std::move(ageMap)),
          platesMap(std::move(platesMap)),
          width(width),
          height(height),
          oceanLevel(oceanLevel) {}


    MapResult(const MapResult& other) = delete;
    MapResult() {
        heights = nullptr;
        ageMap = nullptr;
        platesMap = nullptr;
    };
    MapResult& operator=(const MapResult& other) = delete;

    MapResult(MapResult&& other) noexcept
        : heights(std::move(other.heights)), ageMap(std::move(other.ageMap)), platesMap(std::move(other.platesMap)),
          width(other.width), height(other.height), oceanLevel(other.oceanLevel) {}
    MapResult& operator=(MapResult&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        heights    = std::move(other.heights);
        ageMap     = std::move(other.ageMap);
        platesMap  = std::move(other.platesMap);
        width      = other.width;
        height     = other.height;
        oceanLevel = other.oceanLevel;
        return *this;
    }
};

struct MapArgs {
    u64 seed     = 3;
    u32 width    = 500;
    u32 height   = 500;
    f32 seaLevel = 0.65;

    u32 erosionPeriod  = 60;
    f32 foldingRatio   = 0.02;
    u32 aggrOverlapAbs = 1000000;
    f32 aggrOverlapRel = 0.33;

    u8 cycleCount = 2;
    u8 numPlates  = 10;
};

using ProgressCallback = std::function<void(f32 progress, u8 cycle)>;

class PlatecWrapper {
public:
    /**
     * ==================================================================================
     * =========== Параметры генерации процедурной карты рельефа через platec ===========
     * ==================================================================================
     *
     * Основные параметры:
     * -------------------
     * @param args
     * @param progressCallback
     * @param seed          Случайное зерно генерации. Одинаковый seed даёт идентичные результаты.
     * @param width         Ширина карты в пикселях.
     * @param height        Высота карты в пикселях.
     *
     * Параметры рельефа:
     * ------------------
     * @param seaLevel     Уровень моря [0.0-1.0]. Пример: 0.65 = 65% площади ниже уровня воды.
     * @param erosionPeriod Число итераций эрозии. Чем выше, тем плавнее рельеф.
     *
     * Тектонические параметры:
     * ------------------------
     * @param foldingRatio      Количество тектонических плит (5-15 для реалистичности).
     * @param aggrOverlapAbs   Интенсивность образования гор при столкновении [0.01-0.1].
     *                        Пример: 0.02 = умеренная складчатость.
     * @param aggrOverlapRel Абсолютный порог "слипания" плит (в условных единицах).
     * @param cycleCount Относительный порог перекрытия плит [0.0-1.0].
     *                        Пример: 0.33 = 33% перекрытия для активации эффектов.
     * @param numPlates     Число тектонических циклов (2-5 для баланса скорости/детализации).
     *
     * Примеры настроек:
     * -----------------
     * Континенты: sea_level=0.6, erosion_period=50, num_plates=10
     * Архипелаг:  sea_level=0.8, folding_ratio=0.01
     * Горы:       folding_ratio=0.05, cycle_count=3
     *
     * Примечание:
     * -----------
     * Данный метод, в отдельном потоке, последовательно:
     * 1. Создает плиты
     * 2. Моделирует их движение
     * 3. Рассчитывает рельеф на границах
     * 4. Применяет эрозию
     * 5. Возвращает std::future
     * ===========================================================================
     */
    static MapResult createHeights(const MapArgs& args, const ProgressCallback& progressCallback) noexcept;
};


#endif // GODOT_CPP_TEMPLATE_MAPCREATOR_H
