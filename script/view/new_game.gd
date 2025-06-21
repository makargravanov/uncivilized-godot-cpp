extends Control

var mapCreator = MapCreator.new()

func _ready():
	$Button.pressed.connect(_on_button_pressed)
	mapCreator.set_heightmap_callback(Callable(self, "heightmap_callback"))
	mapCreator.set_finish_callback(Callable(self, "finish_callback"))
	

func heightmap_callback(procent, cycle):
	print("Процент готовности: " + str(procent) + ", цикл: " + str(cycle))

func finish_callback():
	print("Готово")

func _on_button_pressed():
	mapCreator.create()
