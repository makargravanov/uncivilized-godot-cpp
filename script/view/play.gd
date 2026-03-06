extends Node3D

const VIEW_NORMAL := 0
const VIEW_TEMPERATURE := 1
const VIEW_ELEVATION := 3
const VIEW_BIOME := 4
const VIEW_WIND := 5

@onready var playScene = $PlayScene
@onready var viewModeSelect: OptionButton = $CanvasLayer/ViewModePanel/MarginContainer/ViewModeRow/ViewModeSelect

func _ready():
	_setupViewModeSelect()
	_applySelectedViewMode(viewModeSelect.selected)

func _setupViewModeSelect():
	viewModeSelect.clear()
	viewModeSelect.add_item("Обычный", VIEW_NORMAL)
	viewModeSelect.add_item("Температура", VIEW_TEMPERATURE)
	viewModeSelect.add_item("Высота", VIEW_ELEVATION)
	viewModeSelect.add_item("Биомы", VIEW_BIOME)
	viewModeSelect.add_item("Ветер", VIEW_WIND)
	viewModeSelect.item_selected.connect(_on_view_mode_selected)

func _on_view_mode_selected(index: int):
	_applySelectedViewMode(index)

func _applySelectedViewMode(index: int):
	var viewModeId := viewModeSelect.get_item_id(index)
	playScene.set_view_mode(viewModeId)
