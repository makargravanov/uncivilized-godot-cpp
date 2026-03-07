extends Node3D

const VIEW_NORMAL := 0
const VIEW_TEMPERATURE := 1
const VIEW_MOISTURE := 2
const VIEW_ELEVATION := 3
const VIEW_BIOME := 4
const VIEW_WIND := 5

const VIEW_MODE_KEYS := {
	KEY_1: VIEW_NORMAL,
	KEY_2: VIEW_TEMPERATURE,
	KEY_3: VIEW_MOISTURE,
	KEY_4: VIEW_ELEVATION,
	KEY_5: VIEW_BIOME,
	KEY_6: VIEW_WIND,
}

const VIEW_MODE_NAMES := {
	VIEW_NORMAL: "Обычный",
	VIEW_TEMPERATURE: "Температура",
	VIEW_MOISTURE: "Влажность",
	VIEW_ELEVATION: "Высота",
	VIEW_BIOME: "Биомы",
	VIEW_WIND: "Ветер",
}

const RELIEF_NAMES := {
	0: "Океан", 1: "Равнина", 2: "Холм", 3: "Гора", 4: "Высокая гора",
}

const BIOME_NAMES := {
	0: "Глубокий океан", 1: "Мелкий океан", 2: "Внутреннее море",
	3: "Мелк. внутреннее море", 4: "Луг", 5: "Нагорье", 6: "Альпийский",
}

@onready var playScene = $PlayScene
@onready var camera: Camera3D = $Camera3D
@onready var viewModeLabel: Label = $CanvasLayer/ViewModeLabel
@onready var tileInfoLabel: Label = $CanvasLayer/TileInfoLabel

func _ready():
	playScene.set_view_mode(VIEW_NORMAL)
	_updateLabel(VIEW_NORMAL)

func _unhandled_key_input(event: InputEvent):
	if event is InputEventKey and event.pressed and not event.echo:
		if VIEW_MODE_KEYS.has(event.keycode):
			var mode: int = VIEW_MODE_KEYS[event.keycode]
			playScene.set_view_mode(mode)
			_updateLabel(mode)

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
	if info.has("precipitation_annual"):
		lines.append("Осадки (год): %.4f" % info.get("precipitation_annual", 0.0))
	tileInfoLabel.text = "\n".join(lines)

func _updateLabel(mode: int):
	if viewModeLabel:
		viewModeLabel.text = VIEW_MODE_NAMES.get(mode, "")
