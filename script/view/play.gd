extends Node3D

const VIEW_NORMAL := 0
const VIEW_TEMPERATURE := 1
const VIEW_MOISTURE := 2
const VIEW_ELEVATION := 3
const VIEW_BIOME := 4
const VIEW_WIND := 5
const VIEW_PRECIPITATION := 6

const VIEW_MODE_KEYS := {
	KEY_1: VIEW_NORMAL,
	KEY_2: VIEW_TEMPERATURE,
	KEY_3: VIEW_MOISTURE,
	KEY_4: VIEW_ELEVATION,
	KEY_5: VIEW_BIOME,
	KEY_6: VIEW_WIND,
	KEY_7: VIEW_PRECIPITATION,
}

const VIEW_MODE_NAMES := {
	VIEW_NORMAL: "Обычный",
	VIEW_TEMPERATURE: "Температура",
	VIEW_MOISTURE: "Влажность",
	VIEW_ELEVATION: "Высота",
	VIEW_BIOME: "Биомы",
	VIEW_WIND: "Ветер",
	VIEW_PRECIPITATION: "Осадки",
}

const RELIEF_NAMES := {
	0: "Океан", 1: "Равнина", 2: "Холм", 3: "Гора", 4: "Высокая гора",
}

const BIOME_NAMES := {
	0: "Глубокий океан", 1: "Мелкий океан", 2: "Внутреннее море",
	3: "Мелк. внутреннее море", 4: "Полярная пустошь", 5: "Тундра",
	6: "Тайга", 7: "Холодная степь", 8: "Умеренный лес",
	9: "Умеренная степь", 10: "Сухие кустарники", 11: "Жаркая пустыня",
	12: "Саванна", 13: "Сезонный тропический лес",
	14: "Влажный тропический лес", 15: "Альпийский пояс",
}

@onready var playScene = $PlayScene
@onready var camera: Camera3D = $Camera3D
@onready var viewModeLabel: Label = $CanvasLayer/ViewModeLabel
@onready var climateSummaryLabel: Label = $CanvasLayer/ClimateSummaryLabel
@onready var climateRegulatorLabel: Label = $CanvasLayer/ClimateRegulatorLabel
@onready var climateRegulatorOutputsLabel: Label = $CanvasLayer/ClimateRegulatorOutputsLabel
@onready var regulatorEnableCheckBox: CheckBox = $CanvasLayer/RegulatorEnableCheckBox
@onready var regulatorTargetSpinBox: SpinBox = $CanvasLayer/RegulatorTargetSpinBox
@onready var insolationEnableCheckBox: CheckBox = $CanvasLayer/InsolationEnableCheckBox
@onready var insolationStrengthSpinBox: SpinBox = $CanvasLayer/InsolationStrengthSpinBox
@onready var insolationMaxSpinBox: SpinBox = $CanvasLayer/InsolationMaxSpinBox
@onready var cryosphereEnableCheckBox: CheckBox = $CanvasLayer/CryosphereEnableCheckBox
@onready var cryosphereStrengthSpinBox: SpinBox = $CanvasLayer/CryosphereStrengthSpinBox
@onready var cryosphereMaxSpinBox: SpinBox = $CanvasLayer/CryosphereMaxSpinBox
@onready var baseAlbedoEnableCheckBox: CheckBox = $CanvasLayer/BaseAlbedoEnableCheckBox
@onready var baseAlbedoStrengthSpinBox: SpinBox = $CanvasLayer/BaseAlbedoStrengthSpinBox
@onready var baseAlbedoMaxSpinBox: SpinBox = $CanvasLayer/BaseAlbedoMaxSpinBox
@onready var tileInfoLabel: Label = $CanvasLayer/TileInfoLabel
@onready var turnLabel: Label = $CanvasLayer/TurnLabel

var _currentViewMode: int = VIEW_NORMAL

func _ready():
	playScene.set_view_mode(VIEW_NORMAL)
	_syncRegulatorControls()
	regulatorEnableCheckBox.toggled.connect(_on_regulator_enable_check_box_toggled)
	regulatorTargetSpinBox.value_changed.connect(_on_regulator_target_spin_box_value_changed)
	insolationEnableCheckBox.toggled.connect(_on_insolation_enable_check_box_toggled)
	insolationStrengthSpinBox.value_changed.connect(_on_insolation_strength_spin_box_value_changed)
	insolationMaxSpinBox.value_changed.connect(_on_insolation_max_spin_box_value_changed)
	cryosphereEnableCheckBox.toggled.connect(_on_cryosphere_enable_check_box_toggled)
	cryosphereStrengthSpinBox.value_changed.connect(_on_cryosphere_strength_spin_box_value_changed)
	cryosphereMaxSpinBox.value_changed.connect(_on_cryosphere_max_spin_box_value_changed)
	baseAlbedoEnableCheckBox.toggled.connect(_on_base_albedo_enable_check_box_toggled)
	baseAlbedoStrengthSpinBox.value_changed.connect(_on_base_albedo_strength_spin_box_value_changed)
	baseAlbedoMaxSpinBox.value_changed.connect(_on_base_albedo_max_spin_box_value_changed)
	_updateLabel(VIEW_NORMAL)
	_updateTurnLabel()
	_updateClimateSummaryLabel()
	_updateClimateRegulatorLabel()

func _unhandled_key_input(event: InputEvent):
	if event is InputEventKey and event.pressed and not event.echo:
		if VIEW_MODE_KEYS.has(event.keycode):
			var mode: int = VIEW_MODE_KEYS[event.keycode]
			_currentViewMode = mode
			playScene.set_view_mode(mode)
			_updateLabel(mode)

func _process(_delta: float):
	if Input.is_key_pressed(KEY_SPACE):
		playScene.advance_climate_turn()
	_updateTurnLabel()
	_updateClimateSummaryLabel()
	_updateClimateRegulatorLabel()

func _unhandled_input(event: InputEvent):
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		if Input.get_mouse_mode() == Input.MOUSE_MODE_VISIBLE:
			_pickTile(event.position)

func _pickTile(screenPos: Vector2):
	var origin := camera.project_ray_origin(screenPos)
	var direction := camera.project_ray_normal(screenPos)
	# Intersect with y=0 plane
	if abs(direction.y) < 0.0001:
		return
	var t := -origin.y / direction.y
	if t < 0:
		return
	var hit := origin + direction * t
	var info: Dictionary = playScene.get_tile_info_at(hit.x, hit.z)
	if info.is_empty():
		tileInfoLabel.text = ""
		return
	_showTileInfo(info)

func _showTileInfo(info: Dictionary):
	var lines: PackedStringArray = PackedStringArray()
	lines.append("[%d, %d]" % [info.get("col", 0), info.get("row", 0)])
	lines.append("Рельеф: %s" % RELIEF_NAMES.get(info.get("relief", -1), "?"))
	lines.append("Биом: %s" % BIOME_NAMES.get(info.get("biome", -1), "?"))
	lines.append("Температура: %d°C (%.1fK)" % [info.get("temperature_c", 0), info.get("temperature_k", 0.0)])
	if info.has("latitude_deg"):
		lines.append("Широта: %.1f°" % info.get("latitude_deg", 0.0))
	if info.has("altitude"):
		lines.append("Высота: %.3f" % info.get("altitude", 0.0))
	if info.has("wind_speed"):
		lines.append("Ветер: %.1f m/s (E=%.1f, N=%.1f)" % [
			info.get("wind_speed", 0.0),
			info.get("wind_east", 0.0),
			info.get("wind_north", 0.0)])
	if info.has("humidity"):
		lines.append("Влажность: %.4f kg/kg" % info.get("humidity", 0.0))
	if info.has("precipitation_turn"):
		lines.append("Осадки (ход): %.5f" % info.get("precipitation_turn", 0.0))
	if info.has("forest_cover_fraction"):
		lines.append("Лесной покров: %.0f%%" % (info.get("forest_cover_fraction", 0.0) * 100.0))
	if info.has("surface_albedo"):
		lines.append("Альбедо: %.3f" % info.get("surface_albedo", 0.0))
	if info.has("effective_heat_capacity"):
		lines.append("Теплоёмкость поверхности: %.2f" % info.get("effective_heat_capacity", 0.0))
	if info.has("snow_cover_fraction"):
		lines.append("Снежный покров: %.0f%%" % (info.get("snow_cover_fraction", 0.0) * 100.0))
	if info.has("snow_water_equivalent"):
		lines.append("Запас снега: %.4f" % info.get("snow_water_equivalent", 0.0))
	if info.has("sea_ice_fraction"):
		lines.append("Морской лёд: %.0f%%" % (info.get("sea_ice_fraction", 0.0) * 100.0))
	if info.has("soil_water_storage"):
		lines.append("Почвенная влага: %.4f" % info.get("soil_water_storage", 0.0))
	if info.has("soil_water_capacity"):
		lines.append("Емкость почвы: %.4f" % info.get("soil_water_capacity", 0.0))
	if info.has("tile_water_storage_total"):
		lines.append("Вода на тайле: %.4f" % info.get("tile_water_storage_total", 0.0))
	if info.has("precipitation_annual"):
		lines.append("Осадки (год, текущие): %.4f" % info.get("precipitation_annual", 0.0))
	var completed_years: int = int(info.get("climate_years_completed", 0))
	if completed_years > 0:
		lines.append("Завершённых лет климата: %d" % completed_years)
		if info.has("precipitation_annual_completed"):
			lines.append("Осадки (прошлый год): %.4f" % info.get("precipitation_annual_completed", 0.0))
		if info.has("temperature_annual_mean_c"):
			lines.append("T среднегодовая: %.1f°C" % info.get("temperature_annual_mean_c", 0.0))
		if info.has("temperature_annual_min_c") and info.has("temperature_annual_max_c"):
			lines.append("T годовая: %.1f..%.1f°C" % [
				info.get("temperature_annual_min_c", 0.0),
				info.get("temperature_annual_max_c", 0.0)])
		if info.has("temperature_annual_amplitude_c"):
			lines.append("Амплитуда года: %.1f°C" % info.get("temperature_annual_amplitude_c", 0.0))
		if info.has("temperature_coldest_quarter_c"):
			lines.append("T холодной четверти: %.1f°C" % info.get("temperature_coldest_quarter_c", 0.0))
		if info.has("temperature_warmest_quarter_c"):
			lines.append("T тёплой четверти: %.1f°C" % info.get("temperature_warmest_quarter_c", 0.0))
		if info.has("precipitation_driest_quarter"):
			lines.append("Осадки сухой четверти: %.4f" % info.get("precipitation_driest_quarter", 0.0))
		if info.has("precipitation_wettest_quarter"):
			lines.append("Осадки влажной четверти: %.4f" % info.get("precipitation_wettest_quarter", 0.0))
		if info.has("precipitation_seasonality_ratio"):
			lines.append("Контраст осадков: %.1fx" % info.get("precipitation_seasonality_ratio", 0.0))
	tileInfoLabel.text = "\n".join(lines)

func _updateLabel(mode: int):
	if viewModeLabel:
		viewModeLabel.text = VIEW_MODE_NAMES.get(mode, "")

func _updateTurnLabel():
	if turnLabel:
		var turn: int = playScene.get_current_turn()
		if playScene.is_climate_turn_in_progress():
			turnLabel.text = "Ход: %d (считается...)" % turn
		else:
			turnLabel.text = "Ход: %d" % turn

func _updateClimateSummaryLabel():
	if climateSummaryLabel == null:
		return

	var summary: Dictionary = playScene.get_climate_summary()
	if summary.is_empty():
		climateSummaryLabel.text = ""
		return

	var current_years_completed: int = int(summary.get("climate_years_completed", 0))
	var current_temperature_c: float = float(summary.get("current_turn_mean_temperature_c", 0.0))
	var ice_free_temperature_c: float = float(summary.get("current_turn_ice_free_equilibrium_temperature_c", current_temperature_c))
	var cryosphere_cooling_delta_c: float = float(summary.get("current_turn_cryosphere_cooling_delta_c", 0.0))
	var current_cryosphere: float = float(summary.get("current_turn_mean_cryosphere_fraction", 0.0))
	var current_albedo: float = float(summary.get("current_turn_mean_surface_albedo", 0.0))

	var parts: PackedStringArray = PackedStringArray()
	parts.append("Климат")
	parts.append("T факт %.2f°C" % current_temperature_c)
	parts.append("T без крио %.2f°C" % ice_free_temperature_c)
	parts.append("крио %+.2f°C" % cryosphere_cooling_delta_c)
	parts.append("лёд %.0f%%" % (current_cryosphere * 100.0))
	parts.append("альб %.3f" % current_albedo)

	if current_years_completed > 0:
		var delta_c: float = float(summary.get("completed_year_temperature_delta_c", 0.0))
		parts.append("Δ/год %+.3f°C" % delta_c)

	climateSummaryLabel.text = " | ".join(parts)

func _updateClimateRegulatorLabel():
	if climateRegulatorLabel == null:
		return

	var summary: Dictionary = playScene.get_climate_summary()
	if summary.is_empty():
		climateRegulatorLabel.text = ""
		if climateRegulatorOutputsLabel:
			climateRegulatorOutputsLabel.text = ""
		return

	var target_temperature_c: float = float(summary.get("target_global_mean_temperature_c", 14.0))
	var regulator_enabled: bool = bool(summary.get("regulator_correction_enabled", false))
	var controller_temperature_c: float = float(summary.get("controller_mean_temperature_c", 0.0))
	var temperature_error_c: float = float(summary.get("regulator_temperature_error_c", 0.0))
	var trend_c_per_year: float = float(summary.get("controller_trend_c_per_year", 0.0))
	var cryosphere_delta_c: float = float(summary.get("controller_cryosphere_cooling_delta_c", 0.0))
	var demand_normalized: float = float(summary.get("regulator_heating_demand_normalized", 0.0))
	var demand_wm2: float = float(summary.get("regulator_heating_demand_wm2", 0.0))
	var kp: float = float(summary.get("regulator_effective_kp", 0.0))
	var kd: float = float(summary.get("regulator_effective_kd", 0.0))
	var kff: float = float(summary.get("regulator_effective_kff", 0.0))
	var insolation_output_wm2: float = float(summary.get("regulator_insolation_output_wm2", 0.0))
	var cryosphere_output: float = float(summary.get("regulator_cryosphere_albedo_output", 0.0))
	var base_albedo_output: float = float(summary.get("regulator_base_albedo_output", 0.0))

	var parts: PackedStringArray = PackedStringArray()
	parts.append("Regulator")
	parts.append("apply on" if regulator_enabled else "apply off")
	parts.append("target %.1fC" % target_temperature_c)
	parts.append("ctrl T %.2fC" % controller_temperature_c)
	parts.append("err %+.2fC" % temperature_error_c)
	parts.append("trend %+.3fC/yr" % trend_c_per_year)
	parts.append("cryo %+.2fC" % cryosphere_delta_c)
	parts.append("u %+.2f" % demand_normalized)
	parts.append("eq %+.1fW/m2" % demand_wm2)
	parts.append("Kp %.2f" % kp)
	parts.append("Kd %.2f" % kd)
	parts.append("Kff %.2f" % kff)

	climateRegulatorLabel.text = " | ".join(parts)

	if climateRegulatorOutputsLabel:
		var outputParts: PackedStringArray = PackedStringArray()
		outputParts.append("Outputs")
		outputParts.append("insol %+.1fW/m2" % insolation_output_wm2)
		outputParts.append("cryo %+.2f" % cryosphere_output)
		outputParts.append("base %+.3f" % base_albedo_output)
		climateRegulatorOutputsLabel.text = " | ".join(outputParts)

func _syncRegulatorControls():
	if playScene == null:
		return

	if regulatorEnableCheckBox:
		regulatorEnableCheckBox.button_pressed = playScene.is_climate_regulator_correction_enabled()
	if regulatorTargetSpinBox:
		regulatorTargetSpinBox.value = playScene.get_climate_regulator_target_temperature_c()
	if insolationEnableCheckBox:
		insolationEnableCheckBox.button_pressed = playScene.is_climate_regulator_insolation_enabled()
	if insolationStrengthSpinBox:
		insolationStrengthSpinBox.value = playScene.get_climate_regulator_insolation_strength()
	if insolationMaxSpinBox:
		insolationMaxSpinBox.value = playScene.get_climate_regulator_insolation_max_magnitude()
	if cryosphereEnableCheckBox:
		cryosphereEnableCheckBox.button_pressed = playScene.is_climate_regulator_cryosphere_albedo_enabled()
	if cryosphereStrengthSpinBox:
		cryosphereStrengthSpinBox.value = playScene.get_climate_regulator_cryosphere_albedo_strength()
	if cryosphereMaxSpinBox:
		cryosphereMaxSpinBox.value = playScene.get_climate_regulator_cryosphere_albedo_max_magnitude()
	if baseAlbedoEnableCheckBox:
		baseAlbedoEnableCheckBox.button_pressed = playScene.is_climate_regulator_base_albedo_enabled()
	if baseAlbedoStrengthSpinBox:
		baseAlbedoStrengthSpinBox.value = playScene.get_climate_regulator_base_albedo_strength()
	if baseAlbedoMaxSpinBox:
		baseAlbedoMaxSpinBox.value = playScene.get_climate_regulator_base_albedo_max_magnitude()

func _on_regulator_enable_check_box_toggled(button_pressed: bool):
	playScene.set_climate_regulator_correction_enabled(button_pressed)

func _on_regulator_target_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_target_temperature_c(value)

func _on_insolation_enable_check_box_toggled(button_pressed: bool):
	playScene.set_climate_regulator_insolation_enabled(button_pressed)

func _on_insolation_strength_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_insolation_strength(value)

func _on_insolation_max_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_insolation_max_magnitude(value)

func _on_cryosphere_enable_check_box_toggled(button_pressed: bool):
	playScene.set_climate_regulator_cryosphere_albedo_enabled(button_pressed)

func _on_cryosphere_strength_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_cryosphere_albedo_strength(value)

func _on_cryosphere_max_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_cryosphere_albedo_max_magnitude(value)

func _on_base_albedo_enable_check_box_toggled(button_pressed: bool):
	playScene.set_climate_regulator_base_albedo_enabled(button_pressed)

func _on_base_albedo_strength_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_base_albedo_strength(value)

func _on_base_albedo_max_spin_box_value_changed(value: float):
	playScene.set_climate_regulator_base_albedo_max_magnitude(value)
