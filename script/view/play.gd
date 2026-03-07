extends Node3D

const VIEW_NORMAL := 0
const VIEW_TEMPERATURE := 1
const VIEW_ELEVATION := 3
const VIEW_BIOME := 4
const VIEW_WIND := 5

const VIEW_MODE_KEYS := {
	KEY_1: VIEW_NORMAL,
	KEY_2: VIEW_TEMPERATURE,
	KEY_3: VIEW_ELEVATION,
	KEY_4: VIEW_BIOME,
	KEY_5: VIEW_WIND,
}

const VIEW_MODE_NAMES := {
	VIEW_NORMAL: "Обычный",
	VIEW_TEMPERATURE: "Температура",
	VIEW_ELEVATION: "Высота",
	VIEW_BIOME: "Биомы",
	VIEW_WIND: "Ветер",
}

@onready var playScene = $PlayScene
@onready var viewModeLabel: Label = $CanvasLayer/ViewModeLabel

func _ready():
	playScene.set_view_mode(VIEW_NORMAL)
	_updateLabel(VIEW_NORMAL)

func _unhandled_key_input(event: InputEvent):
	if event is InputEventKey and event.pressed and not event.echo:
		if VIEW_MODE_KEYS.has(event.keycode):
			var mode: int = VIEW_MODE_KEYS[event.keycode]
			playScene.set_view_mode(mode)
			_updateLabel(mode)

func _updateLabel(mode: int):
	if viewModeLabel:
		viewModeLabel.text = VIEW_MODE_NAMES.get(mode, "")
