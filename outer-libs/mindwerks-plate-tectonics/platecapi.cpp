/******************************************************************************
 *  plate-tectonics, a plate tectonics simulation library
 *  Copyright (C) 2012-2013 Lauri Viitanen
 *  Copyright (C) 2014-2015 Federico Tomassetti, Bret Curtis
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see http://www.gnu.org/licenses/
 *****************************************************************************/

#include "platecapi.hpp"

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "lithosphere.hpp"
#include "plate.hpp"

class platec_api_list_elem {
public:
    platec_api_list_elem(uint32_t _id, lithosphere* _data) : data(_data), id(_id) {}

    lithosphere* data;
    uint32_t id;

    platec_api_list_elem(const platec_api_list_elem& other) = delete;
    platec_api_list_elem& operator=(const platec_api_list_elem& other) = delete;

    platec_api_list_elem(platec_api_list_elem&& other) noexcept : data(other.data), id(other.id) {
        other.data = nullptr;
    }
    platec_api_list_elem& operator=(platec_api_list_elem&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        data = other.data;
        other.data = nullptr;
        id   = other.id;
        return *this;
    }

    ~platec_api_list_elem() {
        delete data;
    }
};

extern lithosphere* platec_api_get_lithosphere(uint32_t);

static std::vector<platec_api_list_elem> lithospheres;
static uint32_t last_id = 1;

void* platec_api_create(long seed, uint32_t width, uint32_t height, float sea_level, uint32_t erosion_period,
    float folding_ratio, uint32_t aggr_overlap_abs, float aggr_overlap_rel, uint32_t cycle_count, uint32_t num_plates) {
    /* Miten nykyisen opengl-mainin koodit refaktoroidaan tänne?
     *    parametrien tarkistus, kommentit eli dokumentointi, muuta? */

    auto* litho = new lithosphere(seed, width, height, sea_level, erosion_period, folding_ratio, aggr_overlap_abs,
        aggr_overlap_rel, cycle_count, num_plates);

    platec_api_list_elem elem(++last_id, litho);
    lithospheres.emplace_back(std::move(elem));

    return litho;
}

void platec_api_destroy(void* litho) {
    for (uint32_t i = 0; i < lithospheres.size(); ++i) {
        if (lithospheres[i].data == litho) {
            lithospheres.erase(lithospheres.begin() + i);
            break;
        }
    }
}

const uint32_t* platec_api_get_agemap(void* pointer) {
    lithosphere* litho = static_cast<lithosphere*>(pointer);

    return litho->getAgeMap();
}

float* platec_api_get_heightmap(void* pointer) {
    lithosphere* litho = static_cast<lithosphere*>(pointer);
    float* res         = litho->getTopography();
    return res;
}

uint32_t* platec_api_get_platesmap(void* pointer) {
    lithosphere* litho = static_cast<lithosphere*>(pointer);
    uint32_t* res      = litho->getPlatesMap();
    return res;
}

lithosphere* platec_api_get_lithosphere(uint32_t id) {
    for (uint32_t i = 0; i < lithospheres.size(); ++i) {
        if (lithospheres[i].id == id) {
            return lithospheres[i].data;
        }
    }

    return nullptr;
}

uint32_t platec_api_is_finished(void* pointer) {
    lithosphere* litho = (lithosphere*) pointer;
    if (litho->isFinished()) {
        return 1;
    } else {
        return 0;
    }
}

uint32_t platec_api_get_iter_count(void* pointer) {
    lithosphere* litho = (lithosphere*) pointer;
    return litho->getIterationCount();
}

uint32_t platec_api_get_cycle_count(void* pointer) {
    lithosphere* litho = (lithosphere*) pointer;
    return litho->getCycleCount();
}

void platec_api_step(void* pointer) {
    lithosphere* litho = (lithosphere*) pointer;
    litho->update();
}

uint32_t lithosphere_getMapWidth(void* object) {
    return static_cast<lithosphere*>(object)->getWidth();
}

uint32_t lithosphere_getMapHeight(void* object) {
    return static_cast<lithosphere*>(object)->getHeight();
}

float platec_api_velocity_unity_vector_x(void* pointer, uint32_t plate_index) {
    lithosphere* litho = (lithosphere*) pointer;
    return litho->getPlate(plate_index)->velocityUnitVector().x();
}

float platec_api_velocity_unity_vector_y(void* pointer, uint32_t plate_index) {
    lithosphere* litho = (lithosphere*) pointer;
    return litho->getPlate(plate_index)->velocityUnitVector().y();
}
