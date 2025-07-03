extends Node

func _ready():
	var model_path = "res://hexagon_mesh.glb"
	
	var scene = load(model_path)
	var node = scene.instantiate()
	add_child(node)
	
	# Ищем первый MeshInstance3D
	var mesh_instance = find_mesh_instance(node)
	if mesh_instance and mesh_instance.mesh:
		# Сохраняем меш как .tres
		ResourceSaver.save(mesh_instance.mesh, "res://ocean_hexagon_mesh.tres")
		print("Mesh saved as .tres!")
	else:
		printerr("Mesh not found!")
	queue_free()

# Рекурсивный поиск MeshInstance3Dd
func find_mesh_instance(node):
	if node is MeshInstance3D:
		return node
	for child in node.get_children():
		var result = find_mesh_instance(child)
		if result:
			return result
	return null
