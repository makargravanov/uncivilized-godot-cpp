extends Control

func _ready():
	scale_interface()
	$ButtonContainer/NewGame.pressed.connect(_on_new_game_pressed)
	$ButtonContainer/Resume.pressed.connect(_on_continue_pressed)
	$ButtonContainer/LoadGame.pressed.connect(_on_load_pressed)
	$ButtonContainer/Settings.pressed.connect(_on_settings_pressed)
	$ButtonContainer/Exit.pressed.connect(_on_exit_pressed)
	get_viewport().size_changed.connect(scale_interface)

func scale_interface():
	var base_resolution = Vector2(1280, 720) 
	var current_resolution = get_viewport_rect().size
	
	var scale_factor = min(
		current_resolution.x / base_resolution.x,
		current_resolution.y / base_resolution.y
	)

	$ButtonContainer.scale = Vector2(scale_factor, scale_factor)
	
func _on_new_game_pressed():
	print("Запуск новой игры")
	get_tree().change_scene_to_file("res://view/NewGame.tscn")

func _on_continue_pressed():
	print("Продолжить игру")

func _on_load_pressed():
	print("Меню загрузки")

func _on_settings_pressed():
	print("Открыть настройки")

func _on_exit_pressed():
	print("Выход из игры")
	get_tree().quit()
