extends Node3D

# View mode keys: 1 = Normal, 2 = Temperature, 3 = Moisture, 4 = Elevation, 5 = Biome
var current_view_mode: int = 0
var turn_count: int = 0

# UI references (created in _ready)
var info_panel: PanelContainer
var info_label: RichTextLabel
var hud_label: Label
var metrics_panel: PanelContainer
var metrics_label: RichTextLabel
var metrics_visible: bool = false

const VIEW_NAMES := ["Обычный", "Температура", "Осадки", "Высота", "Биом"]

const BIOME_NAMES := [
	"Глубокий океан", "Мелкий океан", "Озеро", "Побережье",
	"Тропический лес", "Тропич. сезонный", "Саванна", "Субтроп. пустыня",
	"Умеренный лес", "Умерен. степь", "Умерен. пустыня",
	"Бореальный лес", "Тундра", "Ледяной щит",
	"Высокогорье", "Альпийский"
]

func _ready() -> void:
	_create_ui()
	_update_hud()

func _create_ui() -> void:
	var canvas := CanvasLayer.new()
	canvas.layer = 10
	add_child(canvas)

	# ── HUD label (top-left): turn counter + view mode ──
	hud_label = Label.new()
	hud_label.position = Vector2(16, 12)
	hud_label.add_theme_font_size_override("font_size", 18)
	hud_label.add_theme_color_override("font_color", Color.WHITE)
	hud_label.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.7))
	hud_label.add_theme_constant_override("shadow_offset_x", 1)
	hud_label.add_theme_constant_override("shadow_offset_y", 1)
	canvas.add_child(hud_label)

	# ── Tile info panel (bottom-left) ──
	info_panel = PanelContainer.new()
	info_panel.position = Vector2(16, 0)  # Y set in _process to anchor to bottom
	info_panel.visible = false

	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.08, 0.08, 0.12, 0.85)
	style.corner_radius_top_left = 6
	style.corner_radius_top_right = 6
	style.corner_radius_bottom_left = 6
	style.corner_radius_bottom_right = 6
	style.content_margin_left = 12
	style.content_margin_right = 12
	style.content_margin_top = 8
	style.content_margin_bottom = 8
	info_panel.add_theme_stylebox_override("panel", style)
	canvas.add_child(info_panel)

	info_label = RichTextLabel.new()
	info_label.bbcode_enabled = true
	info_label.fit_content = true
	info_label.scroll_active = false
	info_label.custom_minimum_size = Vector2(300, 0)
	info_label.add_theme_font_size_override("normal_font_size", 16)
	info_label.add_theme_font_size_override("bold_font_size", 17)
	info_panel.add_child(info_label)

	# ── Climate metrics panel (top-right) ──
	metrics_panel = PanelContainer.new()
	metrics_panel.visible = false

	var mstyle := StyleBoxFlat.new()
	mstyle.bg_color = Color(0.05, 0.08, 0.15, 0.9)
	mstyle.corner_radius_top_left = 6
	mstyle.corner_radius_top_right = 6
	mstyle.corner_radius_bottom_left = 6
	mstyle.corner_radius_bottom_right = 6
	mstyle.content_margin_left = 14
	mstyle.content_margin_right = 14
	mstyle.content_margin_top = 10
	mstyle.content_margin_bottom = 10
	metrics_panel.add_theme_stylebox_override("panel", mstyle)
	canvas.add_child(metrics_panel)

	metrics_label = RichTextLabel.new()
	metrics_label.bbcode_enabled = true
	metrics_label.fit_content = true
	metrics_label.scroll_active = false
	metrics_label.custom_minimum_size = Vector2(380, 0)
	metrics_label.add_theme_font_size_override("normal_font_size", 15)
	metrics_label.add_theme_font_size_override("bold_font_size", 16)
	metrics_panel.add_child(metrics_label)

func _process(_delta: float) -> void:
	# Keep info panel anchored to bottom-left.
	if info_panel and info_panel.visible:
		var vp_h := get_viewport().get_visible_rect().size.y
		info_panel.position.y = vp_h - info_panel.size.y - 16
	# Keep metrics panel anchored to top-right.
	if metrics_panel and metrics_panel.visible:
		var vp_w := get_viewport().get_visible_rect().size.x
		metrics_panel.position = Vector2(vp_w - metrics_panel.size.x - 16, 44)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		var mode := -1
		match event.keycode:
			KEY_1: mode = 0  # Normal
			KEY_2: mode = 1  # Temperature
			KEY_3: mode = 2  # Moisture
			KEY_4: mode = 3  # Elevation
			KEY_5: mode = 4  # Biome
			KEY_SPACE:
				_advance_turn()
				return
			KEY_M:
				_toggle_metrics()
				return
		if mode >= 0 and mode != current_view_mode:
			current_view_mode = mode
			$PlayScene.set_view_mode(mode)
			_update_hud()

	# Tile picking: left-click when mouse is visible (ESC releases camera).
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT:
			if Input.get_mouse_mode() == Input.MOUSE_MODE_VISIBLE:
				_pick_tile(event.position)

func _pick_tile(screen_pos: Vector2) -> void:
	var camera: Camera3D = $Camera3D
	var from := camera.project_ray_origin(screen_pos)
	var dir := camera.project_ray_normal(screen_pos)

	# Intersect ray with y = 0 plane.
	if abs(dir.y) < 0.0001:
		return
	var t := -from.y / dir.y
	if t < 0.0:
		return

	var hit := from + dir * t
	var info: Dictionary = $PlayScene.get_tile_info(hit.x, hit.z)
	if info.is_empty():
		info_panel.visible = false
		return

	_show_tile_info(info)

func _show_tile_info(info: Dictionary) -> void:
	var biome_idx: int = info.get("biome", 0)
	var biome_name: String = BIOME_NAMES[biome_idx] if biome_idx < BIOME_NAMES.size() else "???"
	var temp: float = info.get("temperature", 0.0)
	var temp_annual: float = info.get("temperature_annual", 0.0)
	var precip: float = info.get("precipitation", 0.0)
	var precip_annual: float = info.get("precipitation_annual", 0.0)
	var elev: float = info.get("elevation", 0.0)
	var tx: int = info.get("x", 0)
	var ty: int = info.get("y", 0)

	info_label.text = (
		"[b]%s[/b]\n" % biome_name +
		"Координаты: (%d, %d)\n" % [tx, ty] +
		"Температура: [color=#ffcc44]%.1f °C[/color] (год: %.1f °C)\n" % [temp, temp_annual] +
		"Осадки: [color=#44aaff]%.0f мм/год[/color] (тик: %.1f мм)\n" % [precip_annual, precip] +
		"Высота: %.0f м" % elev
	)
	info_panel.visible = true

func _advance_turn() -> void:
	$PlayScene.advance_turn()
	turn_count += 1
	_update_hud()
	if metrics_visible:
		_update_metrics()

func _toggle_metrics() -> void:
	metrics_visible = not metrics_visible
	if metrics_visible:
		_update_metrics()
	metrics_panel.visible = metrics_visible
	_update_hud()

func _update_metrics() -> void:
	var m: Dictionary = $PlayScene.get_climate_metrics()
	if m.is_empty():
		metrics_label.text = "[i]Нет данных[/i]"
		return

	var imbalance: float = m.get("toa_imbalance", 0.0)
	var imb_color := "gray"
	if imbalance > 1.0:
		imb_color = "#ff6644"  # warming
	elif imbalance < -1.0:
		imb_color = "#4488ff"  # cooling
	else:
		imb_color = "#44ff88"  # near equilibrium

	var dT: float = m.get("dT_mean", 0.0)
	var dT_color := "#44ff88" if abs(dT) < 0.01 else ("#ff6644" if dT > 0 else "#4488ff")

	var year_val: float = m.get("year", 0.0)

	metrics_label.text = (
		"[b]Климатическая модель[/b]  [color=gray]Y=%.2f[/color]\n" % year_val +
		"\n[b]Энергобаланс (TOA)[/b]\n" +
		"  Поглощено: [color=#ffaa44]%.1f[/color] Вт/м²\n" % m.get("absorbed_solar", 0.0) +
		"  Излучено: [color=#4488ff]%.1f[/color] Вт/м²\n" % m.get("emitted_ir", 0.0) +
		"  Дисбаланс: [color=%s]%+.2f Вт/м²[/color]\n" % [imb_color, imbalance] +
		"\n[b]Температура[/b]\n" +
		"  Глобальная: [color=#ffcc44]%.1f °C[/color]\n" % m.get("T_mean_global", 0.0) +
		"  Суша: [color=#ffcc44]%.1f °C[/color]  Океан: [color=#44aaff]%.1f °C[/color]\n" % [m.get("T_mean_land", 0.0), m.get("T_mean_ocean", 0.0)] +
		"  Мин: %.1f °C  Макс: %.1f °C\n" % [m.get("T_min", 0.0), m.get("T_max", 0.0)] +
		"  ΔT/тик: [color=%s]%+.4f °C[/color]\n" % [dT_color, dT] +
		"\n[b]Поверхность[/b]\n" +
		"  Лёд (<0°C): %.1f%%\n" % (m.get("ice_fraction", 0.0) * 100.0) +
		"  Снег суши (<−5°C): %.1f%%\n" % (m.get("snow_fraction", 0.0) * 100.0) +
		"  Альбедо: %.3f\n" % m.get("albedo_mean", 0.0) +
		"\n[b]Влага[/b]\n" +
		"  Ср. влажность: %.3f\n" % m.get("moisture_mean", 0.0) +
		"  Осадки (суша): [color=#44aaff]%.0f мм/год[/color]" % m.get("precip_mean_land", 0.0)
	)

func _update_hud() -> void:
	var view_name: String = VIEW_NAMES[current_view_mode] if current_view_mode < VIEW_NAMES.size() else "?"
	hud_label.text = "Ход: %d  |  Вид: %s  |  [Пробел] — ход  |  [M] — метрики  |  [ESC] — мышь" % [turn_count, view_name]
