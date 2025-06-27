extends Camera3D

@export var move_speed: float = 5.0
@export var look_sensitivity: float = 0.2

signal player_position_updated(new_position: Vector3)

var rotation_y: float = 0
var rotation_x: float = 0

func _ready():
	# Capture mouse (hides and locks to center of window)
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _input(event):
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		rotation_y -= event.relative.x * look_sensitivity
		rotation_x -= event.relative.y * look_sensitivity
		rotation_x = clamp(rotation_x, -90, 90)
		
		transform.basis = Basis()
		rotate_object_local(Vector3.UP, deg_to_rad(rotation_y))
		rotate_object_local(Vector3.RIGHT, deg_to_rad(rotation_x))
	
	if event.is_action_pressed("ui_cancel"):
		# Toggle mouse capture when ESC pressed
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		else:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _process(delta):
	# Movement controls
	var input_vector = Vector3.ZERO
	input_vector.x = Input.get_action_strength("move_right") - Input.get_action_strength("move_left")
	input_vector.z = Input.get_action_strength("move_back") - Input.get_action_strength("move_forward")
	input_vector.y = Input.get_action_strength("move_up") - Input.get_action_strength("move_down")
	
	if input_vector != Vector3.ZERO:
		var direction = (transform.basis * input_vector).normalized()
		position += direction * move_speed * delta
		emit_signal("player_position_updated", position)
