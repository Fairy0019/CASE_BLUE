extends Node3D

const Game = preload("res://scripts/chess_game.gd")
const AI = preload("res://scripts/chess_ai.gd")

const STAGE_SCENE := "res://ChessSet.blend"
const SQUARE := 0.056
const BOARD_TOP := 0.022
const PLAY_HALF := 0.224
const MODE_AI := 0
const MODE_LOCAL := 1

const WHITE := 0
const BLACK := 1

const TYPE_BY_NAME := {
	"Pawn": 1, "Knight": 2, "Bishop": 3, "Rook": 4, "Queen": 5, "King": 6,
}
const TYPE_NAME := ["", "Pawn", "Knight", "Bishop", "Rook", "Queen", "King"]
const TYPE_CN := ["", "兵", "马", "象", "车", "后", "王"]

const COL_GOLD := Color(0.82, 0.65, 0.37)
const COL_CREAM := Color(0.92, 0.87, 0.77)
const COL_MUTED := Color(0.64, 0.58, 0.48)
const COL_DANGER := Color(0.92, 0.43, 0.34)
const COL_PANEL := Color(0.055, 0.050, 0.044, 0.93)
const COL_PANEL_SOFT := Color(0.075, 0.068, 0.058, 0.92)
const COL_BORDER := Color(0.55, 0.44, 0.26, 0.35)
const COL_BTN := Color(0.115, 0.100, 0.083, 0.95)
const COL_BTN_HOVER := Color(0.17, 0.145, 0.115, 0.98)
const COL_BTN_PRESS := Color(0.26, 0.21, 0.14, 1.0)

const BASE_YAW := -0.34
const BASE_PITCH := 0.74
const BASE_DIST := 0.86
const AI_DEPTH := 3

var game
var ai = AI.new()

var mode := MODE_AI
var player_side := WHITE
var session_id := 0
var is_busy := false
var ai_thinking := false
var promotion_open := false
var gameover_open := false
var selected_sq := -1
var legal_by_target: Dictionary = {}
var promotion_moves: Array = []

var board_root: Node3D
var pieces_root: Node3D
var selection_root: Node3D
var last_move_root: Node3D
var piece_nodes: Dictionary = {}
var protos: Dictionary = {}

var cam: Camera3D
var cam_yaw := BASE_YAW
var cam_pitch := BASE_PITCH
var cam_dist := BASE_DIST
var cam_target := Vector3(0.0, -0.024, 0.0)
var cam_tween: Tween
var dragging := false
var last_mouse := Vector2.ZERO

var mat_white: StandardMaterial3D
var mat_black: StandardMaterial3D
var mat_light_sq: ShaderMaterial
var mat_dark_sq: ShaderMaterial
var mat_frame: ShaderMaterial
var mat_core: StandardMaterial3D
var mat_felt: StandardMaterial3D
var mat_brass: StandardMaterial3D
var mat_last_move: StandardMaterial3D
var mat_select: StandardMaterial3D
var mat_move_dot: StandardMaterial3D
var mat_capture: StandardMaterial3D
var mat_check: StandardMaterial3D

var ui_root: Control
var turn_dot: Panel
var turn_label: Label
var turn_sub: Label
var mode_caption: Label
var mode_local_btn: Button
var mode_ai_btn: Button
var swap_btn: Button
var undo_btn: Button
var new_btn: Button
var flip_btn: Button
var move_text: RichTextLabel
var captured_label: Label
var promotion_overlay: Control
var promotion_panel: PanelContainer
var gameover_overlay: Control
var gameover_panel: PanelContainer
var gameover_result: Label
var gameover_reason: Label
var gameover_buttons: HBoxContainer

var font_ui: SystemFont
var font_title: SystemFont
var font_mono: SystemFont
var theme: Theme

var sfx_player: AudioStreamPlayer
var sfx_players: Array[AudioStreamPlayer] = []
var sfx := {}
var active_tween: Tween
var move_tween: Tween


func _ready() -> void:
	randomize()
	_make_materials()
	_setup_world()
	_load_stage()
	_build_ui()
	_make_audio()
	_new_game(MODE_AI, BLACK if "--black" in OS.get_cmdline_user_args() else WHITE)
	var args := OS.get_cmdline_user_args()
	if "--promo" in args:
		call_deferred("_debug_promo")
	elif "--mate" in args:
		call_deferred("_debug_mate")
	elif "--demo" in args:
		call_deferred("_debug_demo")
	elif "--undo" in args:
		call_deferred("_debug_undo")
	elif "--black" in args:
		call_deferred("_debug_black")
	elif "--auto" in args:
		call_deferred("_debug_auto")
	elif "--shot" in args:
		call_deferred("_debug_capture")

# ----------------------------------------------------------------------------- world
func _setup_world() -> void:
	var env_node := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.012, 0.011, 0.010)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.58, 0.66, 0.82)
	env.ambient_light_energy = 0.42
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_exposure = 0.95
	env.ssao_enabled = true
	env.ssao_radius = 0.11
	env.ssao_intensity = 1.6
	env.ssao_power = 1.2
	env.ssao_light_affect = 0.25
	env.glow_enabled = true
	env.glow_intensity = 0.28
	env.glow_bloom = 0.04
	env.glow_hdr_threshold = 1.0
	env.adjustment_enabled = true
	env.adjustment_contrast = 1.04
	env.adjustment_saturation = 1.03
	env_node.environment = env
	add_child(env_node)

	var key := DirectionalLight3D.new()
	key.name = "KeyLight"
	key.rotation_degrees = Vector3(-54, -37, 0)
	key.light_color = Color(1.0, 0.91, 0.78)
	key.light_energy = 0.98
	key.shadow_enabled = true
	key.shadow_bias = 0.025
	key.shadow_normal_bias = 1.2
	key.directional_shadow_max_distance = 4.0
	key.shadow_blur = 1.2
	add_child(key)

	var fill := OmniLight3D.new()
	fill.name = "FillLight"
	fill.position = Vector3(0.55, 0.48, 0.55)
	fill.light_color = Color(0.58, 0.72, 1.0)
	fill.light_energy = 0.75
	fill.omni_range = 2.4
	fill.shadow_enabled = false
	add_child(fill)

	var rim := OmniLight3D.new()
	rim.name = "RimLight"
	rim.position = Vector3(-0.52, 0.48, -0.58)
	rim.light_color = Color(1.0, 0.74, 0.48)
	rim.light_energy = 0.85
	rim.omni_range = 2.1
	rim.shadow_enabled = false
	add_child(rim)

	var top := OmniLight3D.new()
	top.name = "TopLight"
	top.position = Vector3(0.0, 1.0, 0.10)
	top.light_color = Color(1.0, 0.95, 0.86)
	top.light_energy = 0.30
	top.omni_range = 2.2
	top.shadow_enabled = false
	add_child(top)

	var left_fill := OmniLight3D.new()
	left_fill.name = "LeftFill"
	left_fill.position = Vector3(-0.52, 0.34, 0.18)
	left_fill.light_color = Color(0.68, 0.78, 1.0)
	left_fill.light_energy = 0.55
	left_fill.omni_range = 1.8
	left_fill.shadow_enabled = false
	add_child(left_fill)

	var floor_mesh := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(12.0, 12.0)
	floor_mesh.mesh = plane
	var floor_mat := ShaderMaterial.new()
	floor_mat.shader = load("res://assets/shaders/table.gdshader")
	floor_mat.set_shader_parameter("center_color", Color(0.065, 0.055, 0.045))
	floor_mat.set_shader_parameter("edge_color", Color(0.008, 0.008, 0.009))
	floor_mat.set_shader_parameter("pool_radius", 1.15)
	floor_mesh.material_override = floor_mat
	floor_mesh.position.y = -0.004
	floor_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(floor_mesh)

	cam = Camera3D.new()
	cam.name = "Camera"
	cam.fov = 46.0
	cam.near = 0.01
	cam.far = 30.0
	cam.current = true
	add_child(cam)
	_update_camera()


func _make_materials() -> void:
	mat_white = StandardMaterial3D.new()
	mat_white.albedo_color = Color(0.47, 0.40, 0.30)
	mat_white.roughness = 0.45
	mat_white.metallic = 0.0
	mat_white.clearcoat_enabled = true
	mat_white.clearcoat = 0.03
	mat_white.clearcoat_roughness = 0.18

	mat_black = StandardMaterial3D.new()
	mat_black.albedo_color = Color(0.030, 0.025, 0.021)
	mat_black.roughness = 0.27
	mat_black.metallic = 0.06
	mat_black.clearcoat_enabled = true
	mat_black.clearcoat = 0.32
	mat_black.clearcoat_roughness = 0.12

	var wood_shader: Shader = load("res://assets/shaders/wood.gdshader")
	mat_light_sq = ShaderMaterial.new()
	mat_light_sq.shader = wood_shader
	mat_light_sq.set_shader_parameter("light_wood", Color(0.34, 0.255, 0.165))
	mat_light_sq.set_shader_parameter("dark_wood", Color(0.155, 0.095, 0.052))
	mat_light_sq.set_shader_parameter("grain_strength", 0.34)
	mat_light_sq.set_shader_parameter("grain_scale", 1350.0)
	mat_light_sq.set_shader_parameter("roughness_value", 0.58)

	mat_dark_sq = ShaderMaterial.new()
	mat_dark_sq.shader = wood_shader
	mat_dark_sq.set_shader_parameter("light_wood", Color(0.16, 0.090, 0.048))
	mat_dark_sq.set_shader_parameter("dark_wood", Color(0.055, 0.027, 0.013))
	mat_dark_sq.set_shader_parameter("grain_strength", 0.40)
	mat_dark_sq.set_shader_parameter("grain_scale", 1350.0)
	mat_dark_sq.set_shader_parameter("roughness_value", 0.55)

	mat_frame = ShaderMaterial.new()
	mat_frame.shader = wood_shader
	mat_frame.set_shader_parameter("light_wood", Color(0.095, 0.049, 0.026))
	mat_frame.set_shader_parameter("dark_wood", Color(0.024, 0.011, 0.005))
	mat_frame.set_shader_parameter("grain_strength", 0.40)
	mat_frame.set_shader_parameter("grain_scale", 1000.0)
	mat_frame.set_shader_parameter("roughness_value", 0.55)

	mat_core = StandardMaterial3D.new()
	mat_core.albedo_color = Color(0.020, 0.014, 0.010)
	mat_core.roughness = 0.82

	mat_felt = StandardMaterial3D.new()
	mat_felt.albedo_color = Color(0.018, 0.045, 0.026)
	mat_felt.roughness = 0.95

	mat_brass = StandardMaterial3D.new()
	mat_brass.albedo_color = Color(0.64, 0.44, 0.15)
	mat_brass.metallic = 0.9
	mat_brass.roughness = 0.30

	mat_last_move = _overlay_mat(Color(0.95, 0.71, 0.35, 0.13))
	mat_select = _overlay_mat(Color(0.98, 0.76, 0.36, 0.62))
	mat_move_dot = _overlay_mat(Color(0.63, 0.88, 0.72, 0.58))
	mat_capture = _overlay_mat(Color(0.95, 0.42, 0.28, 0.72))
	mat_check = _overlay_mat(Color(0.95, 0.22, 0.20, 0.66))


func _overlay_mat(color: Color) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	m.albedo_color = color
	m.no_depth_test = false
	m.render_priority = 3
	return m


func _setup_highlight_roots() -> void:
	last_move_root = Node3D.new()
	last_move_root.name = "LastMoveMarkers"
	add_child(last_move_root)
	selection_root = Node3D.new()
	selection_root.name = "SelectionMarkers"
	add_child(selection_root)


func _load_stage() -> void:
	board_root = Node3D.new()
	board_root.name = "Board"
	add_child(board_root)
	pieces_root = Node3D.new()
	pieces_root.name = "Pieces"
	add_child(pieces_root)
	_setup_highlight_roots()

	var packed: PackedScene = load(STAGE_SCENE)
	if packed == null:
		push_error("Could not load chess stage")
		return
	var source: Node3D = packed.instantiate()
	var board_names := [
		"Chessboard_Frame", "Chessboard_Tiles", "Chessboard_Core",
		"Chessboard_Feet", "Chessboard_Labels",
	]
	for n in board_names:
		var node := source.get_node_or_null(n)
		if node:
			source.remove_child(node)
			node.owner = null
			board_root.add_child(node)
	_apply_board_materials()

	for child in source.get_children():
		if not (child is MeshInstance3D):
			continue
		var nm := String(child.name)
		if not (nm.begins_with("White_") or nm.begins_with("Black_")):
			continue
		var parts := nm.split("_")
		if parts.size() < 2:
			continue
		var side := WHITE if parts[0] == "White" else BLACK
		var type_name := parts[1]
		if not TYPE_BY_NAME.has(type_name):
			continue
		var type: int = TYPE_BY_NAME[type_name]
		var key := "%d_%d" % [side, type]
		if protos.has(key):
			continue
		var mesh_instance := child as MeshInstance3D
		if mesh_instance.mesh == null:
			continue
		protos[key] = {
			"mesh": mesh_instance.mesh,
			"rotation": mesh_instance.rotation,
			"scale": mesh_instance.scale,
			"aabb": mesh_instance.get_aabb(),
		}

	source.free()


func _apply_board_materials() -> void:
	var frame := board_root.get_node_or_null("Chessboard_Frame") as MeshInstance3D
	if frame:
		frame.material_override = mat_frame
		frame.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	var tiles := board_root.get_node_or_null("Chessboard_Tiles") as MeshInstance3D
	if tiles:
		tiles.set_surface_override_material(0, mat_light_sq)
		tiles.set_surface_override_material(1, mat_dark_sq)
		tiles.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	var core := board_root.get_node_or_null("Chessboard_Core") as MeshInstance3D
	if core:
		core.material_override = mat_core
		core.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	var feet := board_root.get_node_or_null("Chessboard_Feet") as MeshInstance3D
	if feet:
		feet.material_override = mat_felt
		feet.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var labels := board_root.get_node_or_null("Chessboard_Labels") as MeshInstance3D
	if labels:
		labels.material_override = mat_brass
		labels.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF


# ----------------------------------------------------------------------------- pieces
func _piece_world(square: int) -> Vector3:
	var file := square & 7
	var rank := square >> 3
	return Vector3((file - 3.5) * SQUARE, BOARD_TOP, (3.5 - rank) * SQUARE)


func _square_name(square: int) -> String:
	return char(97 + (square & 7)) + str((square >> 3) + 1)


func _spawn_piece(side: int, type: int, square: int) -> Node3D:
	var key := "%d_%d" % [side, type]
	if not protos.has(key):
		return null
	var proto: Dictionary = protos[key]
	var root := Node3D.new()
	root.name = "Piece_%s_%s_%s" % ["White" if side == WHITE else "Black", TYPE_NAME[type], _square_name(square)]
	root.position = _piece_world(square)
	root.rotation = proto["rotation"]
	root.scale = proto["scale"]
	root.set_meta("square", square)
	root.set_meta("side", side)
	root.set_meta("type", type)

	var mesh := MeshInstance3D.new()
	mesh.name = "Mesh"
	mesh.mesh = proto["mesh"]
	mesh.material_override = mat_white if side == WHITE else mat_black
	mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	root.add_child(mesh)

	var aabb: AABB = proto["aabb"]
	var body := StaticBody3D.new()
	body.name = "Picker"
	body.collision_layer = 1
	body.collision_mask = 0
	root.add_child(body)
	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = aabb.size
	col.shape = box
	col.position = aabb.get_center()
	body.add_child(col)

	pieces_root.add_child(root)
	piece_nodes[square] = root
	return root


func _rebuild_pieces() -> void:
	if board_root == null:
		return
	for child in pieces_root.get_children():
		pieces_root.remove_child(child)
		child.queue_free()
	piece_nodes.clear()
	if game == null:
		return
	for square in range(64):
		var code: int = game.board[square]
		if code != 0:
			_spawn_piece(code >> 3, code & 7, square)


# ----------------------------------------------------------------------------- camera
func _update_camera() -> void:
	if cam == null:
		return
	var view_target := cam_target + Vector3(cos(cam_yaw), 0.0, -sin(cam_yaw)) * 0.095
	var offset := Vector3(
		sin(cam_yaw) * cos(cam_pitch),
		sin(cam_pitch),
		cos(cam_yaw) * cos(cam_pitch)
	) * cam_dist
	cam.position = view_target + offset
	cam.look_at(view_target, Vector3.UP)


func _set_view_side(side: int, animated: bool) -> void:
	var target_yaw := BASE_YAW + (PI if side == BLACK else 0.0)
	if animated:
		_tween_camera(target_yaw, 0.55)
	else:
		cam_yaw = target_yaw
		_update_camera()


func _tween_camera(target_yaw: float, duration: float) -> void:
	if cam_tween and cam_tween.is_valid():
		cam_tween.kill()
	cam_tween = create_tween()
	cam_tween.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
	cam_tween.tween_method(_cam_yaw_step, cam_yaw, target_yaw, duration)


func _cam_yaw_step(v: float) -> void:
	cam_yaw = v
	_update_camera()


func _cam_pitch_step(v: float) -> void:
	cam_pitch = v
	_update_camera()


func _cam_dist_step(v: float) -> void:
	cam_dist = v
	_update_camera()


func _flip_view() -> void:
	_tween_camera(cam_yaw + PI, 0.55)


func _reset_view() -> void:
	if cam_tween and cam_tween.is_valid():
		cam_tween.kill()
	cam_tween = create_tween().set_parallel(true)
	cam_tween.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
	var target_yaw := BASE_YAW + (PI if mode == MODE_AI and player_side == BLACK else 0.0)
	cam_tween.tween_method(_cam_yaw_step, cam_yaw, target_yaw, 0.55)
	cam_tween.tween_method(_cam_pitch_step, cam_pitch, BASE_PITCH, 0.55)
	cam_tween.tween_method(_cam_dist_step, cam_dist, BASE_DIST, 0.55)


# ----------------------------------------------------------------------------- game flow
func _new_game(new_mode: int = -1, new_side: int = -1) -> void:
	session_id += 1
	if new_mode >= 0:
		mode = new_mode
	if new_side >= 0:
		player_side = new_side
	ai_thinking = false
	is_busy = false
	promotion_open = false
	gameover_open = false
	selected_sq = -1
	legal_by_target.clear()
	promotion_moves.clear()
	if move_tween and move_tween.is_valid():
		move_tween.kill()
	_hide_overlays()
	game = Game.new()
	_rebuild_pieces()
	_update_last_move_markers()
	_update_highlights()
	_set_view_side(player_side if mode == MODE_AI else WHITE, false)
	_update_ui()
	if mode == MODE_AI and game.turn != player_side:
		_start_ai_turn()


func _start_ai_turn() -> void:
	if game == null or game.game_over or mode != MODE_AI:
		return
	if game.turn == player_side:
		return
	var sid := session_id
	ai_thinking = true
	is_busy = true
	_update_ui()
	await get_tree().create_timer(0.30).timeout
	if sid != session_id or game == null or game.game_over:
		if sid == session_id:
			ai_thinking = false
			is_busy = false
			_update_ui()
		return
	var best: Dictionary = ai.find_best_move(game, AI_DEPTH)
	if sid != session_id or game == null:
		return
	ai_thinking = false
	is_busy = false
	if best.is_empty():
		_update_ui()
		_show_game_over()
		return
	_execute_move(best)


func _on_board_click(pos: Vector2) -> void:
	if game == null or game.game_over or is_busy or promotion_open or gameover_open:
		return
	if mode == MODE_AI and game.turn != player_side:
		return
	var square := _pick_square(pos)
	if square < 0:
		_clear_selection()
		return
	if selected_sq < 0:
		if _is_own_piece(square):
			_select_square(square)
		return
	if square == selected_sq:
		_clear_selection()
		return
	if legal_by_target.has(square):
		_play_target(square)
		return
	if _is_own_piece(square):
		_select_square(square)
	else:
		_clear_selection()


func _is_own_piece(square: int) -> bool:
	if game == null or square < 0 or square > 63:
		return false
	var code: int = game.board[square]
	if code == 0:
		return false
	if (code >> 3) != game.turn:
		return false
	if mode == MODE_AI and game.turn != player_side:
		return false
	return true


func _select_square(square: int) -> void:
	selected_sq = square
	legal_by_target.clear()
	var moves: Array = game.legal_moves(game.turn)
	for mv in moves:
		if mv["from"] != square:
			continue
		var to: int = mv["to"]
		if not legal_by_target.has(to):
			legal_by_target[to] = []
		legal_by_target[to].append(mv)
	_update_highlights()
	_update_ui()


func _clear_selection() -> void:
	selected_sq = -1
	legal_by_target.clear()
	_update_highlights()
	_update_ui()


func _play_target(square: int) -> void:
	var moves: Array = legal_by_target[square]
	if moves.is_empty():
		return
	var has_promo := false
	for mv in moves:
		if mv.get("promo", 0) != 0:
			has_promo = true
			break
	if has_promo:
		promotion_moves = moves.duplicate(true)
		_show_promotion()
	else:
		var mv: Dictionary = moves[0]
		_execute_move(mv)


func _execute_move(mv: Dictionary) -> void:
	if game == null or game.game_over or is_busy:
		return
	is_busy = true
	promotion_open = false
	var sid := session_id
	_clear_selection()
	_hide_promotion()
	var info: Dictionary = game.make_move(mv)
	if active_tween and active_tween.is_valid():
		active_tween.kill()

	var captured_node: Node3D = null
	if info["captured_sq"] >= 0:
		captured_node = piece_nodes.get(info["captured_sq"])
		piece_nodes.erase(info["captured_sq"])

	var node: Node3D = piece_nodes.get(info["from"])
	if node == null:
		_rebuild_pieces()
		_after_move(sid)
		return
	piece_nodes.erase(info["from"])
	piece_nodes[info["to"]] = node
	node.set_meta("square", info["to"])

	var rook_node: Node3D = null
	if info["rook_from"] >= 0:
		rook_node = piece_nodes.get(info["rook_from"])
		piece_nodes.erase(info["rook_from"])
		if rook_node:
			piece_nodes[info["rook_to"]] = rook_node
			rook_node.set_meta("square", info["rook_to"])

	var target := _piece_world(info["to"])
	var hop := 0.034 if (info["type"] == Game.KNIGHT) else 0.009
	var start := node.position
	active_tween = create_tween()
	active_tween.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
	active_tween.tween_method(
		_piece_step.bind(node, start, target, hop),
		0.0, 1.0, 0.27)
	if rook_node:
		var rt := create_tween()
		rt.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
		var rook_start := rook_node.position
		var rook_target := _piece_world(info["rook_to"])
		rt.tween_method(
			_piece_step.bind(rook_node, rook_start, rook_target, 0.006),
			0.0, 1.0, 0.27)
	if captured_node:
		_animate_capture(captured_node)

	if info["flag"] == "castle_k" or info["flag"] == "castle_q":
		_sfx("castle")
	elif info["captured"] != 0:
		_sfx("capture")
	else:
		_sfx("move")
	await active_tween.finished
	if sid != session_id:
		return

	if info["promo"] != 0:
		piece_nodes.erase(info["to"])
		if is_instance_valid(node):
			node.queue_free()
		var new_piece := _spawn_piece(game.piece_colour(game.board[info["to"]]),
			game.piece_type(game.board[info["to"]]), info["to"])
		if new_piece:
			new_piece.scale = Vector3.ONE
		_sfx("promote")

	if captured_node and is_instance_valid(captured_node):
		captured_node.queue_free()

	is_busy = false
	_update_last_move_markers()
	_update_highlights()
	_update_ui()
	_after_move(sid)


func _after_move(sid: int) -> void:
	is_busy = false
	_update_ui()
	if game == null or sid != session_id:
		return
	if game.game_over:
		await get_tree().create_timer(0.22).timeout
		if sid == session_id:
			_show_game_over()
		return
	if game.in_check(game.turn):
		_sfx("check")
	if mode == MODE_AI and game.turn != player_side:
		_start_ai_turn()


func _animate_capture(node: Node3D) -> void:
	var t := create_tween().set_parallel(true)
	t.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN)
	t.tween_property(node, "scale", Vector3(0.01, 0.01, 0.01), 0.20)
	t.tween_property(node, "position:y", node.position.y + 0.035, 0.20)


func _piece_step(v: float, node: Node3D, start: Vector3, target: Vector3, hop: float) -> void:
	if not is_instance_valid(node):
		return
	var p := start.lerp(target, v)
	p.y += sin(v * PI) * hop
	node.position = p


func _undo() -> void:
	if game == null or game.history.is_empty() or is_busy or ai_thinking:
		return
	if mode == MODE_AI and game.history.size() < 2:
		return
	var count := 1
	if mode == MODE_AI:
		count = 2
	for i in range(count):
		if not game.undo():
			break
	gameover_open = false
	_hide_overlays()
	_clear_selection()
	_rebuild_pieces()
	_update_last_move_markers()
	_update_highlights()
	_update_ui()
	_sfx("move")


# ----------------------------------------------------------------------------- picking
func _pick_square(pos: Vector2) -> int:
	if cam == null:
		return -1
	var plane_sq := _pick_plane_square(pos)
	var piece_sq := _pick_piece_square(pos)
	if plane_sq >= 0:
		var occupied: bool = game != null and game.board[plane_sq] != 0
		if occupied or legal_by_target.has(plane_sq):
			return plane_sq
	if piece_sq >= 0:
		return piece_sq
	return plane_sq


func _pick_piece_square(pos: Vector2) -> int:
	if cam == null or game == null:
		return -1
	var from := cam.project_ray_origin(pos)
	var dir := cam.project_ray_normal(pos)
	var query := PhysicsRayQueryParameters3D.create(from, from + dir * 3.0)
	query.collision_mask = 1
	query.collide_with_areas = false
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if not hit.is_empty():
		var collider = hit.get("collider")
		if collider is StaticBody3D:
			var parent: Node = collider.get_parent()
			if parent and parent.has_meta("square"):
				return int(parent.get_meta("square"))
	return -1


func _pick_plane_square(pos: Vector2) -> int:
	if cam == null:
		return -1
	var from := cam.project_ray_origin(pos)
	var dir := cam.project_ray_normal(pos)
	var plane := Plane(Vector3.UP, BOARD_TOP)
	var point = plane.intersects_ray(from, dir)
	if point == null:
		return -1
	var p: Vector3 = point
	if abs(p.x) > PLAY_HALF or abs(p.z) > PLAY_HALF:
		return -1
	var file := int(round(p.x / SQUARE + 3.5))
	var rank := int(round(3.5 - p.z / SQUARE))
	if file < 0 or file > 7 or rank < 0 or rank > 7:
		return -1
	return rank * 8 + file


func _hover_cursor(pos: Vector2) -> void:
	if game == null or game.game_over or is_busy or promotion_open or gameover_open:
		Input.set_default_cursor_shape(Input.CURSOR_ARROW)
		return
	var square := _pick_square(pos)
	if square < 0:
		Input.set_default_cursor_shape(Input.CURSOR_ARROW)
		return
	if legal_by_target.has(square) or _is_own_piece(square):
		Input.set_default_cursor_shape(Input.CURSOR_POINTING_HAND)
	else:
		Input.set_default_cursor_shape(Input.CURSOR_ARROW)


# ----------------------------------------------------------------------------- highlights
func _update_highlights() -> void:
	if selection_root == null:
		return
	for c in selection_root.get_children():
		selection_root.remove_child(c)
		c.queue_free()
	if game == null or game.game_over:
		return
	if selected_sq >= 0:
		_add_ring(selection_root, selected_sq, mat_select, 0.020, 0.026, 0.024)
		for to in legal_by_target.keys():
			var is_capture: bool = game.board[to] != 0
			if is_capture:
				_add_ring(selection_root, to, mat_capture, 0.016, 0.023, 0.020)
			else:
				_add_dot(selection_root, to, mat_move_dot, 0.0065)
	if not game.game_over and game.in_check(game.turn):
		var king: int = game.find_king(game.turn)
		if king >= 0:
			_add_ring(selection_root, king, mat_check, 0.022, 0.030, 0.018)


func _update_last_move_markers() -> void:
	if last_move_root == null:
		return
	for c in last_move_root.get_children():
		last_move_root.remove_child(c)
		c.queue_free()
	if game == null or game.last_move.is_empty():
		return
	_add_square_plane(last_move_root, game.last_move["from"], mat_last_move)
	_add_square_plane(last_move_root, game.last_move["to"], mat_last_move)


func _add_square_plane(root: Node3D, square: int, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = Vector2(SQUARE * 0.92, SQUARE * 0.92)
	mi.mesh = pm
	mi.material_override = mat
	mi.position = _piece_world(square) + Vector3(0, 0.00035, 0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(mi)


func _add_dot(root: Node3D, square: int, mat: Material, radius: float) -> void:
	var mi := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = radius
	cm.bottom_radius = radius
	cm.height = 0.0006
	cm.radial_segments = 32
	mi.mesh = cm
	mi.material_override = mat
	mi.position = _piece_world(square) + Vector3(0, 0.0007, 0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(mi)


func _add_ring(root: Node3D, square: int, mat: Material, inner: float, outer: float, y: float) -> void:
	var mi := MeshInstance3D.new()
	var tm := TorusMesh.new()
	tm.inner_radius = inner
	tm.outer_radius = outer
	tm.rings = 48
	mi.mesh = tm
	mi.material_override = mat
	mi.position = _piece_world(square) + Vector3(0, 0.0009, 0)
	mi.scale.y = 0.10
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(mi)


# ----------------------------------------------------------------------------- UI
func _make_fonts() -> void:
	font_ui = SystemFont.new()
	font_ui.font_names = PackedStringArray([
		"Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC",
		"SimHei", "Segoe UI", "Arial",
	])
	font_ui.allow_system_fallback = true

	font_title = SystemFont.new()
	font_title.font_names = PackedStringArray(["Georgia", "Times New Roman", "Cambria", "serif"])
	font_title.font_weight = 700
	font_title.allow_system_fallback = true

	font_mono = SystemFont.new()
	font_mono.font_names = PackedStringArray(["Cascadia Mono", "Consolas", "Courier New", "monospace"])
	font_mono.allow_system_fallback = true


func _make_theme() -> Theme:
	var t := Theme.new()
	t.default_font = font_ui
	t.default_font_size = 14

	var normal := _style(COL_BTN, COL_BORDER, 9, 1, 12, 9)
	var hover := _style(COL_BTN_HOVER, Color(0.72, 0.58, 0.35, 0.55), 9, 1, 12, 9)
	var pressed := _style(COL_GOLD, Color(0.96, 0.81, 0.52), 9, 1, 12, 9)
	var disabled := _style(Color(0.075, 0.068, 0.058, 0.7), Color(0.25, 0.22, 0.17, 0.25), 9, 1, 12, 9)
	t.set_stylebox("normal", "Button", normal)
	t.set_stylebox("hover", "Button", hover)
	t.set_stylebox("pressed", "Button", pressed)
	t.set_stylebox("disabled", "Button", disabled)
	t.set_color("font_color", "Button", COL_CREAM)
	t.set_color("font_hover_color", "Button", Color(1.0, 0.96, 0.87))
	t.set_color("font_pressed_color", "Button", Color(0.10, 0.085, 0.065))
	t.set_color("font_disabled_color", "Button", Color(0.45, 0.41, 0.35))
	t.set_font_size("font_size", "Button", 14)

	var panel_style := _style(COL_PANEL, COL_BORDER, 16, 1, 18, 18)
	panel_style.shadow_color = Color(0, 0, 0, 0.38)
	panel_style.shadow_size = 14
	panel_style.shadow_offset = Vector2(0, 6)
	t.set_stylebox("panel", "PanelContainer", panel_style)
	return t


func _style(bg: Color, border: Color, radius: int, border_width: int,
		margin_h: int, margin_v: int) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.border_color = border
	sb.set_border_width_all(border_width)
	sb.set_corner_radius_all(radius)
	sb.content_margin_left = margin_h
	sb.content_margin_right = margin_h
	sb.content_margin_top = margin_v
	sb.content_margin_bottom = margin_v
	return sb


func _label(text: String, size: int, color: Color, font: Font = null) -> Label:
	var l := Label.new()
	l.text = text
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	if font:
		l.add_theme_font_override("font", font)
	return l


func _button(text: String) -> Button:
	var b := Button.new()
	b.text = text
	b.focus_mode = Control.FOCUS_NONE
	b.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	b.custom_minimum_size.y = 34
	return b


func _separator() -> ColorRect:
	var line := ColorRect.new()
	line.color = Color(0.64, 0.52, 0.32, 0.20)
	line.custom_minimum_size.y = 1
	line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return line


func _build_ui() -> void:
	_make_fonts()
	theme = _make_theme()

	var layer := CanvasLayer.new()
	layer.name = "UI"
	add_child(layer)
	ui_root = Control.new()
	ui_root.name = "Root"
	ui_root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	ui_root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_root.theme = theme
	layer.add_child(ui_root)

	_build_brand()
	_build_side_panel()
	_build_hint()
	_build_promotion_overlay()
	_build_gameover_overlay()


func _build_brand() -> void:
	var box := VBoxContainer.new()
	box.position = Vector2(30, 26)
	box.size = Vector2(360, 130)
	box.add_theme_constant_override("separation", 4)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_root.add_child(box)

	var title := _label("CHESS", 38, COL_GOLD, font_title)
	box.add_child(title)
	var line := ColorRect.new()
	line.color = Color(0.82, 0.65, 0.37, 0.8)
	line.custom_minimum_size = Vector2(44, 2)
	line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(line)
	var sub := _label("国 际 象 棋 · 3D 棋 局", 12, COL_MUTED)
	sub.add_theme_constant_override("line_spacing", 3)
	box.add_child(sub)
	var edition := _label("GODOT EDITION", 9, Color(0.48, 0.43, 0.35))
	box.add_child(edition)


func _build_side_panel() -> void:
	var panel := PanelContainer.new()
	panel.name = "SidePanel"
	panel.anchor_left = 1.0
	panel.anchor_right = 1.0
	panel.anchor_top = 0.0
	panel.anchor_bottom = 1.0
	panel.offset_left = -332
	panel.offset_right = -24
	panel.offset_top = 24
	panel.offset_bottom = -24
	panel.mouse_filter = Control.MOUSE_FILTER_STOP
	ui_root.add_child(panel)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 11)
	panel.add_child(vbox)

	# turn card
	var turn_card := PanelContainer.new()
	var tc_style := _style(COL_PANEL_SOFT, Color(0.58, 0.46, 0.28, 0.22), 12, 1, 14, 12)
	turn_card.add_theme_stylebox_override("panel", tc_style)
	vbox.add_child(turn_card)
	var turn_row := HBoxContainer.new()
	turn_row.add_theme_constant_override("separation", 12)
	turn_card.add_child(turn_row)
	turn_dot = Panel.new()
	turn_dot.custom_minimum_size = Vector2(20, 20)
	turn_dot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	turn_dot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	turn_row.add_child(turn_dot)
	var turn_col := VBoxContainer.new()
	turn_col.add_theme_constant_override("separation", 2)
	turn_col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	turn_row.add_child(turn_col)
	turn_label = _label("白方走棋", 20, COL_CREAM)
	turn_col.add_child(turn_label)
	turn_sub = _label("轮到你落子", 12, COL_MUTED)
	turn_col.add_child(turn_sub)

	# modes
	var mode_row := HBoxContainer.new()
	mode_row.add_theme_constant_override("separation", 6)
	vbox.add_child(mode_row)
	var group := ButtonGroup.new()
	mode_local_btn = _button("双人")
	mode_ai_btn = _button("人机")
	mode_local_btn.toggle_mode = true
	mode_ai_btn.toggle_mode = true
	mode_local_btn.button_group = group
	mode_ai_btn.button_group = group
	mode_local_btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	mode_ai_btn.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	mode_local_btn.pressed.connect(_on_mode_pressed.bind(MODE_LOCAL))
	mode_ai_btn.pressed.connect(_on_mode_pressed.bind(MODE_AI))
	mode_row.add_child(mode_local_btn)
	mode_row.add_child(mode_ai_btn)
	swap_btn = _button("换边")
	swap_btn.custom_minimum_size.x = 58
	swap_btn.tooltip_text = "交换人机对战的执棋方"
	swap_btn.pressed.connect(_on_swap_pressed)
	mode_row.add_child(swap_btn)

	mode_caption = _label("人机对战 · 你执白", 11, COL_MUTED)
	vbox.add_child(mode_caption)
	vbox.add_child(_separator())

	# move history
	var hist_head := HBoxContainer.new()
	vbox.add_child(hist_head)
	var hist_title := _label("棋谱", 13, COL_CREAM)
	hist_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hist_head.add_child(hist_title)
	var hist_small := _label("自动滚动", 10, Color(0.45, 0.41, 0.35))
	hist_head.add_child(hist_small)

	var move_panel := PanelContainer.new()
	var mp_style := _style(Color(0.030, 0.027, 0.023, 0.72), Color(0.55, 0.44, 0.26, 0.16), 10, 1, 10, 8)
	move_panel.add_theme_stylebox_override("panel", mp_style)
	move_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	move_panel.custom_minimum_size = Vector2(0, 220)
	vbox.add_child(move_panel)
	move_text = RichTextLabel.new()
	move_text.bbcode_enabled = true
	move_text.scroll_active = true
	move_text.scroll_following = true
	move_text.fit_content = false
	move_text.mouse_filter = Control.MOUSE_FILTER_PASS
	move_text.add_theme_font_override("normal_font", font_mono)
	move_text.add_theme_font_size_override("normal_font_size", 13)
	move_text.add_theme_color_override("default_color", COL_CREAM)
	move_text.text = "[color=#6b6253]尚未落子[/color]"
	move_panel.add_child(move_text)

	captured_label = _label("白提：—   黑提：—", 11, COL_MUTED)
	captured_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	captured_label.custom_minimum_size.y = 30
	vbox.add_child(captured_label)

	var btn_row := HBoxContainer.new()
	btn_row.add_theme_constant_override("separation", 6)
	vbox.add_child(btn_row)
	undo_btn = _button("悔棋")
	new_btn = _button("新局")
	flip_btn = _button("翻转")
	for b in [undo_btn, new_btn, flip_btn]:
		b.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		btn_row.add_child(b)
	undo_btn.tooltip_text = "撤销上一步（Ctrl+Z）"
	new_btn.tooltip_text = "重新开始（N）"
	flip_btn.tooltip_text = "翻转视角（F）"
	undo_btn.pressed.connect(_undo)
	new_btn.pressed.connect(func(): _new_game(mode, player_side))
	flip_btn.pressed.connect(_flip_view)
	new_btn.add_theme_color_override("font_color", COL_GOLD)


func _build_hint() -> void:
	var box := VBoxContainer.new()
	box.anchor_left = 0.0
	box.anchor_top = 1.0
	box.anchor_right = 0.0
	box.anchor_bottom = 1.0
	box.offset_left = 30
	box.offset_top = -58
	box.offset_right = 520
	box.offset_bottom = -22
	box.add_theme_constant_override("separation", 2)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_root.add_child(box)
	var h1 := _label("左键选子 / 落子     右键拖拽旋转     滚轮缩放", 11, Color(0.56, 0.51, 0.43))
	box.add_child(h1)
	var h2 := _label("Ctrl+Z 悔棋     N 新局     F 翻转视角     R 重置视角", 10, Color(0.40, 0.37, 0.32))
	box.add_child(h2)


func _build_promotion_overlay() -> void:
	promotion_overlay = Control.new()
	promotion_overlay.name = "PromotionOverlay"
	promotion_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	promotion_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	promotion_overlay.visible = false
	ui_root.add_child(promotion_overlay)

	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.56)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	promotion_overlay.add_child(dim)

	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	promotion_overlay.add_child(center)

	promotion_panel = PanelContainer.new()
	var style := _style(COL_PANEL, COL_GOLD, 16, 1, 26, 22)
	style.shadow_color = Color(0, 0, 0, 0.5)
	style.shadow_size = 24
	promotion_panel.add_theme_stylebox_override("panel", style)
	center.add_child(promotion_panel)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 8)
	promotion_panel.add_child(v)
	var title := _label("兵 升 变", 24, COL_CREAM, font_title)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(title)
	var sub := _label("选择升变棋子", 11, COL_MUTED)
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(sub)

	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	v.add_child(row)
	var promo_data := [
		[Game.QUEEN, "后", "Queen"],
		[Game.ROOK, "车", "Rook"],
		[Game.BISHOP, "象", "Bishop"],
		[Game.KNIGHT, "马", "Knight"],
	]
	for item in promo_data:
		var b := _button(item[1])
		b.custom_minimum_size = Vector2(66, 64)
		b.add_theme_font_size_override("font_size", 24)
		b.tooltip_text = item[2]
		b.pressed.connect(_on_promote_pressed.bind(item[0]))
		row.add_child(b)

	var cancel := _button("取消")
	cancel.custom_minimum_size = Vector2(0, 30)
	cancel.add_theme_font_size_override("font_size", 12)
	cancel.pressed.connect(_cancel_promotion)
	v.add_child(cancel)


func _build_gameover_overlay() -> void:
	gameover_overlay = Control.new()
	gameover_overlay.name = "GameOverOverlay"
	gameover_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	gameover_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	gameover_overlay.visible = false
	ui_root.add_child(gameover_overlay)

	var dim := ColorRect.new()
	dim.color = Color(0, 0, 0, 0.58)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	gameover_overlay.add_child(dim)

	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	gameover_overlay.add_child(center)

	gameover_panel = PanelContainer.new()
	var style := _style(COL_PANEL, COL_GOLD, 18, 1, 36, 30)
	style.shadow_color = Color(0, 0, 0, 0.55)
	style.shadow_size = 26
	gameover_panel.add_theme_stylebox_override("panel", style)
	center.add_child(gameover_panel)

	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 10)
	gameover_panel.add_child(v)
	var small := _label("对 局 结 束", 11, COL_MUTED)
	small.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(small)
	gameover_result = _label("白方胜利", 34, COL_CREAM, font_title)
	gameover_result.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(gameover_result)
	gameover_reason = _label("将死", 12, COL_MUTED)
	gameover_reason.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	v.add_child(gameover_reason)
	var line := _separator()
	line.custom_minimum_size = Vector2(240, 1)
	v.add_child(line)
	gameover_buttons = HBoxContainer.new()
	gameover_buttons.add_theme_constant_override("separation", 10)
	gameover_buttons.alignment = BoxContainer.ALIGNMENT_CENTER
	v.add_child(gameover_buttons)
	var again := _button("再来一局")
	again.custom_minimum_size = Vector2(126, 38)
	again.pressed.connect(func(): _new_game(mode, player_side))
	gameover_buttons.add_child(again)
	var inspect := _button("查看棋盘")
	inspect.custom_minimum_size = Vector2(126, 38)
	inspect.pressed.connect(_hide_gameover)
	gameover_buttons.add_child(inspect)


# ----------------------------------------------------------------------------- UI updates
func _update_ui() -> void:
	if game == null or turn_label == null:
		return
	var side_name := "白方" if game.turn == WHITE else "黑方"
	var dot_color := Color(0.93, 0.88, 0.76) if game.turn == WHITE else Color(0.12, 0.10, 0.085)
	var dot_border := Color(0.82, 0.65, 0.37, 0.85) if game.turn == WHITE else Color(0.78, 0.62, 0.38, 1.0)
	var dot_style := _style(dot_color, dot_border, 99, 2 if game.turn == BLACK else 1, 0, 0)
	turn_dot.add_theme_stylebox_override("panel", dot_style)

	if game.game_over:
		turn_label.text = "对局结束"
		turn_sub.text = game.result_text
		turn_sub.add_theme_color_override("font_color", COL_GOLD)
	elif ai_thinking:
		turn_label.text = side_name + "走棋"
		turn_sub.text = "AI 正在思考…"
		turn_sub.add_theme_color_override("font_color", COL_GOLD)
	elif game.in_check(game.turn):
		turn_label.text = side_name + "走棋"
		turn_sub.text = "将军！请应将"
		turn_sub.add_theme_color_override("font_color", COL_DANGER)
	elif selected_sq >= 0:
		turn_label.text = side_name + "走棋"
		turn_sub.text = "已选 " + _square_name(selected_sq) + " · 点击目标格"
		turn_sub.add_theme_color_override("font_color", COL_GOLD)
	else:
		turn_label.text = side_name + "走棋"
		if mode == MODE_LOCAL:
			turn_sub.text = "双人同屏 · 请落子"
			turn_sub.add_theme_color_override("font_color", COL_MUTED)
		elif game.turn == player_side:
			turn_sub.text = "轮到你落子"
			turn_sub.add_theme_color_override("font_color", COL_MUTED)
		else:
			turn_sub.text = "AI 回合"
			turn_sub.add_theme_color_override("font_color", COL_MUTED)

	if mode == MODE_AI:
		mode_caption.text = "人机对战 · 你执" + ("白" if player_side == WHITE else "黑")
	else:
		mode_caption.text = "双人对弈 · 同屏轮流"

	if mode_local_btn and mode_ai_btn:
		mode_local_btn.set_pressed_no_signal(mode == MODE_LOCAL)
		mode_ai_btn.set_pressed_no_signal(mode == MODE_AI)
		var lock := is_busy or ai_thinking
		mode_local_btn.disabled = lock
		mode_ai_btn.disabled = lock
		swap_btn.disabled = lock
		swap_btn.visible = mode == MODE_AI
		swap_btn.text = "换边"

	undo_btn.disabled = is_busy or ai_thinking or game.history.is_empty() \
		or (mode == MODE_AI and game.history.size() < 2)
	new_btn.disabled = is_busy or ai_thinking
	flip_btn.disabled = false

	move_text.text = _build_move_log()
	captured_label.text = _build_captured()
	_scroll_moves.call_deferred()


func _build_move_log() -> String:
	if game.move_log.is_empty():
		return "[color=#6b6253]尚未落子[/color]"
	var lines: Array[String] = []
	var i := 0
	while i < game.move_log.size():
		var e: Dictionary = game.move_log[i]
		if e["side"] == WHITE:
			var line := "[color=#716656]%d.[/color] [color=#efe6d6]%s[/color]" % [e["number"], e["text"]]
			if i + 1 < game.move_log.size() and game.move_log[i + 1]["side"] == BLACK:
				line += "    [color=#c7b28d]%s[/color]" % game.move_log[i + 1]["text"]
				i += 2
			else:
				i += 1
			lines.append(line)
		else:
			lines.append("[color=#716656]%d…[/color] [color=#c7b28d]%s[/color]" % [e["number"], e["text"]])
			i += 1
	return "\n".join(lines)


func _scroll_moves() -> void:
	if move_text and move_text.get_line_count() > 0:
		move_text.scroll_to_line(move_text.get_line_count() - 1)


func _build_captured() -> String:
	if game == null:
		return ""
	var w := _capture_counts(game.captured_by_white)
	var b := _capture_counts(game.captured_by_black)
	return "白提：" + (w if not w.is_empty() else "—") + "    黑提：" + (b if not b.is_empty() else "—")


func _capture_counts(codes: Array) -> String:
	var counts := {1: 0, 2: 0, 3: 0, 4: 0, 5: 0}
	for code in codes:
		var t: int = code & 7
		if counts.has(t):
			counts[t] += 1
	var parts: Array[String] = []
	for t in [1, 2, 3, 4, 5]:
		if counts[t] > 0:
			parts.append(TYPE_CN[t] + "×" + str(counts[t]))
	return " ".join(parts)


func _show_promotion() -> void:
	promotion_open = true
	promotion_overlay.visible = true
	promotion_overlay.modulate.a = 0.0
	var t := create_tween()
	t.tween_property(promotion_overlay, "modulate:a", 1.0, 0.16)
	turn_sub.text = "选择升变棋子"
	turn_sub.add_theme_color_override("font_color", COL_GOLD)


func _hide_promotion() -> void:
	promotion_overlay.visible = false
	promotion_open = false


func _cancel_promotion() -> void:
	promotion_moves.clear()
	_hide_promotion()
	_update_highlights()
	_update_ui()


func _on_promote_pressed(type: int) -> void:
	if not promotion_open:
		return
	var chosen: Dictionary = {}
	for mv in promotion_moves:
		if mv.get("promo", 0) == type:
			chosen = mv
			break
	if chosen.is_empty() and not promotion_moves.is_empty():
		chosen = promotion_moves[0].duplicate(true)
		chosen["promo"] = type
	promotion_moves.clear()
	_hide_promotion()
	selected_sq = -1
	legal_by_target.clear()
	_update_highlights()
	if not chosen.is_empty():
		_execute_move(chosen)


func _hide_overlays() -> void:
	if promotion_overlay:
		promotion_overlay.visible = false
	if gameover_overlay:
		gameover_overlay.visible = false
	promotion_open = false
	gameover_open = false


func _show_game_over() -> void:
	if game == null or not game.game_over or gameover_open:
		return
	gameover_open = true
	gameover_result.text = game.result_text
	var reason: String = game.result_reason
	match reason:
		"checkmate":
			gameover_reason.text = "将死 · 对局结束"
		"stalemate":
			gameover_reason.text = "逼和 · 无合法着法"
		"50回合规则":
			gameover_reason.text = "50 回合规则 · 和棋"
		"子力不足":
			gameover_reason.text = "子力不足 · 和棋"
		_:
			gameover_reason.text = reason
	gameover_overlay.visible = true
	gameover_overlay.modulate.a = 0.0
	var t := create_tween()
	t.tween_property(gameover_overlay, "modulate:a", 1.0, 0.22)
	_play_end_sound()


func _hide_gameover() -> void:
	gameover_open = false
	gameover_overlay.visible = false


func _play_end_sound() -> void:
	if game.game_over:
		if game.result_reason == "checkmate":
			if mode == MODE_AI:
				if game.result_text.contains("白方") and player_side == WHITE:
					_sfx("win")
				elif game.result_text.contains("黑方") and player_side == BLACK:
					_sfx("win")
				else:
					_sfx("lose")
			else:
				_sfx("win")
		else:
			_sfx("draw")


func _on_mode_pressed(m: int) -> void:
	if is_busy or ai_thinking:
		_update_ui()
		return
	if m == mode:
		return
	_new_game(m, player_side if m == MODE_AI else WHITE)


func _on_swap_pressed() -> void:
	if is_busy or ai_thinking:
		return
	if mode != MODE_AI:
		return
	_new_game(MODE_AI, BLACK if player_side == WHITE else WHITE)


# ----------------------------------------------------------------------------- input
func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			_on_board_click(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			dragging = event.pressed
			last_mouse = event.position
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			cam_dist = clampf(cam_dist - 0.055, 0.50, 1.6)
			_update_camera()
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			cam_dist = clampf(cam_dist + 0.055, 0.50, 1.6)
			_update_camera()
	elif event is InputEventMouseMotion:
		if dragging and (event.button_mask & MOUSE_BUTTON_MASK_RIGHT):
			cam_yaw -= event.relative.x * 0.006
			cam_pitch = clampf(cam_pitch + event.relative.y * 0.005, 0.22, 1.20)
			_update_camera()
		else:
			_hover_cursor(event.position)


func _unhandled_key_input(event: InputEvent) -> void:
	if not (event is InputEventKey) or not event.pressed or event.echo:
		return
	match event.keycode:
		KEY_ESCAPE:
			if promotion_open:
				_cancel_promotion()
			elif gameover_open:
				_hide_gameover()
			else:
				_clear_selection()
		KEY_Z:
			if event.ctrl_pressed:
				_undo()
		KEY_N:
			if not is_busy and not ai_thinking:
				_new_game(mode, player_side)
		KEY_F:
			_flip_view()
		KEY_R:
			_reset_view()


# ----------------------------------------------------------------------------- audio
func _make_audio() -> void:
	for name in ["move", "capture", "check", "castle", "promote", "win", "lose", "draw"]:
		var path := "res://assets/audio/%s.wav" % name
		sfx[name] = load(path)
	sfx_player = AudioStreamPlayer.new()
	sfx_player.volume_db = -7.0
	add_child(sfx_player)


func _sfx(name: String) -> void:
	if sfx.get(name) == null:
		return
	sfx_player.stream = sfx[name]
	sfx_player.play()


# ----------------------------------------------------------------------------- debug screenshot
func _debug_capture() -> void:
	await get_tree().create_timer(1.1).timeout
	if "--select" in OS.get_cmdline_user_args():
		_select_square(12)  # e2
		await get_tree().create_timer(0.25).timeout
	var path := "main.png"
	if "--select" in OS.get_cmdline_user_args():
		path = "selected.png"
	if "--auto" in OS.get_cmdline_user_args():
		path = "auto.png"
	await _save_shot(path)


func _save_shot(file_name: String) -> void:
	await RenderingServer.frame_post_draw
	var dir := "res://screenshots"
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dir))
	var img := get_viewport().get_texture().get_image()
	var path := dir + "/" + file_name
	img.save_png(path)
	print("SCREENSHOT ", path)
	get_tree().quit()


func _debug_black() -> void:
	await get_tree().create_timer(2.8).timeout
	await _save_shot("black_view.png")


func _apply_demo_move(from_name: String, to_name: String) -> bool:
	for mv in game.legal_moves():
		if game._square_name(mv["from"]) == from_name and game._square_name(mv["to"]) == to_name:
			game.make_move(mv)
			return true
	push_warning("Demo move not found: " + from_name + to_name)
	return false


func _debug_demo() -> void:
	await get_tree().create_timer(0.8).timeout
	mode = MODE_LOCAL
	player_side = WHITE
	game = Game.new()
	var seq := [
		["e2", "e4"], ["e7", "e5"], ["g1", "f3"], ["b8", "c6"],
		["f1", "b5"], ["a7", "a6"], ["b5", "a4"], ["g8", "f6"],
		["e1", "g1"], ["f8", "e7"], ["f1", "e1"], ["b7", "b5"],
		["a4", "b3"], ["d7", "d6"], ["c2", "c3"], ["e8", "g8"],
	]
	for pair in seq:
		_apply_demo_move(pair[0], pair[1])
	selected_sq = -1
	legal_by_target.clear()
	_rebuild_pieces()
	_update_last_move_markers()
	_select_square(21)  # Nf3
	await get_tree().create_timer(0.5).timeout
	await _save_shot("demo.png")


func _debug_promo() -> void:
	await get_tree().create_timer(0.8).timeout
	game = Game.new("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")
	selected_sq = -1
	legal_by_target.clear()
	_rebuild_pieces()
	_update_last_move_markers()
	_update_ui()
	_select_square(48)  # a7
	_play_target(56)    # a8, opens the promotion dialog
	if "--promo-play" in OS.get_cmdline_user_args():
		await get_tree().create_timer(0.5).timeout
		_on_promote_pressed(Game.QUEEN)
		await get_tree().create_timer(1.2).timeout
		print("PROMO RESULT a8=", game.board[56], " nodes=", piece_nodes.has(56))
		await _save_shot("promotion_play.png")
	else:
		await get_tree().create_timer(0.6).timeout
		await _save_shot("promotion.png")


func _debug_mate() -> void:
	await get_tree().create_timer(0.8).timeout
	game = Game.new("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3")
	game._update_status()
	selected_sq = -1
	legal_by_target.clear()
	_rebuild_pieces()
	_update_last_move_markers()
	_update_ui()
	_show_game_over()
	await get_tree().create_timer(0.8).timeout
	await _save_shot("gameover.png")


func _debug_auto() -> void:
	await get_tree().create_timer(0.8).timeout
	var p1 := cam.unproject_position(_piece_world(12))
	var p2 := cam.unproject_position(_piece_world(28))
	print("AUTO pick e2=", _pick_square(p1), " e4=", _pick_square(p2))
	_on_board_click(p1)
	await get_tree().create_timer(0.30).timeout
	print("AUTO selected=", selected_sq, " targets=", legal_by_target.keys())
	_on_board_click(p2)
	await get_tree().create_timer(3.2).timeout
	print("AUTO log=", game.move_log)
	print("AUTO turn=", game.turn, " ai_thinking=", ai_thinking, " busy=", is_busy)
	await _debug_capture()


func _debug_undo() -> void:
	await get_tree().create_timer(0.8).timeout
	var p1 := cam.unproject_position(_piece_world(12))
	var p2 := cam.unproject_position(_piece_world(28))
	_on_board_click(p1)
	await get_tree().create_timer(0.25).timeout
	_on_board_click(p2)
	await get_tree().create_timer(2.8).timeout
	print("UNDO before log=", game.move_log.size(), " history=", game.history.size(), " turn=", game.turn)
	_undo()
	await get_tree().create_timer(0.7).timeout
	print("UNDO after log=", game.move_log.size(), " history=", game.history.size(),
		" turn=", game.turn, " pieces=", piece_nodes.size())
	await _save_shot("undo.png")







