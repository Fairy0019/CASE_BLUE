class_name ChessGame
extends RefCounted

# Piece codes: low 3 bits = type, bit 3 = colour
const WHITE := 0
const BLACK := 1

const PAWN := 1
const KNIGHT := 2
const BISHOP := 3
const ROOK := 4
const QUEEN := 5
const KING := 6

const START_FEN := "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

const CASTLE_WK := 1
const CASTLE_WQ := 2
const CASTLE_BK := 4
const CASTLE_BQ := 8

const KNIGHT_DELTAS := [[1, 2], [2, 1], [2, -1], [1, -2], [-1, -2], [-2, -1], [-2, 1], [-1, 2]]
const BISHOP_DIRS := [[1, 1], [1, -1], [-1, 1], [-1, -1]]
const ROOK_DIRS := [[1, 0], [-1, 0], [0, 1], [0, -1]]
const KING_DIRS := [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]
const PROMO_TYPES := [QUEEN, ROOK, BISHOP, KNIGHT]

var board: Array = []
var turn := WHITE
var castling := 15
var ep_square := -1
var halfmove := 0
var fullmove := 1

var history: Array = []
var move_log: Array = []
var captured_by_white: Array = []   # black pieces captured by white
var captured_by_black: Array = []   # white pieces captured by black
var last_move: Dictionary = {}
var game_over := false
var result_text := ""
var result_reason := ""


func _init(fen: String = "") -> void:
	if fen.is_empty():
		setup_start()
	else:
		load_fen(fen)


func setup_start() -> void:
	_setup_empty()
	load_fen(START_FEN)


func _setup_empty() -> void:
	board.clear()
	board.resize(64)
	board.fill(0)
	turn = WHITE
	castling = 15
	ep_square = -1
	halfmove = 0
	fullmove = 1
	history.clear()
	move_log.clear()
	captured_by_white.clear()
	captured_by_black.clear()
	last_move = {}
	game_over = false
	result_text = ""
	result_reason = ""


func load_fen(fen: String) -> void:
	_setup_empty()
	var parts := fen.strip_edges().split(" ", false)
	if parts.size() < 4:
		push_error("Invalid FEN: " + fen)
		return
	var rows := parts[0].split("/")
	var rank := 7
	for row in rows:
		var file := 0
		for ch in row:
			if ch >= "1" and ch <= "8":
				file += ch.to_int()
			else:
				var code := _char_to_piece(ch)
				if code != 0 and file < 8 and rank >= 0:
					board[rank * 8 + file] = code
				file += 1
		rank -= 1
	turn = WHITE if parts[1] == "w" else BLACK
	castling = 0
	if parts[2] != "-":
		if parts[2].contains("K"): castling |= CASTLE_WK
		if parts[2].contains("Q"): castling |= CASTLE_WQ
		if parts[2].contains("k"): castling |= CASTLE_BK
		if parts[2].contains("q"): castling |= CASTLE_BQ
	ep_square = _square_from_name(parts[3]) if parts[3] != "-" else -1
	halfmove = int(parts[4]) if parts.size() > 4 else 0
	fullmove = int(parts[5]) if parts.size() > 5 else 1


func to_fen() -> String:
	var rows: Array[String] = []
	for rank in range(7, -1, -1):
		var row := ""
		var empty := 0
		for file in range(8):
			var code: int = board[rank * 8 + file]
			if code == 0:
				empty += 1
			else:
				if empty > 0:
					row += str(empty)
					empty = 0
				row += _piece_to_char(code)
		if empty > 0:
			row += str(empty)
		rows.append(row)
	var c := ""
	if castling & CASTLE_WK: c += "K"
	if castling & CASTLE_WQ: c += "Q"
	if castling & CASTLE_BK: c += "k"
	if castling & CASTLE_BQ: c += "q"
	if c.is_empty(): c = "-"
	return "%s %s %s %s %d %d" % [
		"/".join(rows), ("w" if turn == WHITE else "b"), c,
		_square_name(ep_square) if ep_square >= 0 else "-", halfmove, fullmove
	]


# ----------------------------------------------------------------------------- helpers
func piece_type(code: int) -> int:
	return code & 7


func piece_colour(code: int) -> int:
	return code >> 3


func make_piece(colour: int, type: int) -> int:
	return type | (colour << 3)


func sq(file: int, rank: int) -> int:
	return rank * 8 + file


func file_of(square: int) -> int:
	return square & 7


func rank_of(square: int) -> int:
	return square >> 3


func _on(file: int, rank: int) -> bool:
	return file >= 0 and file < 8 and rank >= 0 and rank < 8


func _square_name(square: int) -> String:
	if square < 0 or square > 63:
		return "-"
	return char(97 + file_of(square)) + str(rank_of(square) + 1)


func _square_from_name(name: String) -> int:
	if name.length() < 2:
		return -1
	var f := name.unicode_at(0) - 97
	var r := name.unicode_at(1) - 49
	if not _on(f, r):
		return -1
	return sq(f, r)


func _char_to_piece(ch: String) -> int:
	var colour := WHITE if ch == ch.to_upper() else BLACK
	var t := 0
	match ch.to_lower():
		"p": t = PAWN
		"n": t = KNIGHT
		"b": t = BISHOP
		"r": t = ROOK
		"q": t = QUEEN
		"k": t = KING
		_: return 0
	return make_piece(colour, t)


func _piece_to_char(code: int) -> String:
	const letters := ["", "P", "N", "B", "R", "Q", "K"]
	var s: String = str(letters[piece_type(code)])
	return s if piece_colour(code) == WHITE else s.to_lower()


func opposite(colour: int) -> int:
	return BLACK if colour == WHITE else WHITE


# ----------------------------------------------------------------------------- move generation
func legal_moves(colour: int = -1) -> Array:
	if colour < 0:
		colour = turn
	var result: Array = []
	for mv in pseudo_moves(colour):
		var snap := _min_snapshot()
		_apply_move(mv)
		var ok := not in_check(colour)
		_restore_min(snap)
		if ok:
			result.append(mv)
	return result


func pseudo_moves(colour: int = -1) -> Array:
	if colour < 0:
		colour = turn
	var moves: Array = []
	for square in range(64):
		var code: int = board[square]
		if code == 0 or piece_colour(code) != colour:
			continue
		var type := piece_type(code)
		match type:
			PAWN:
				_pawn_moves(square, colour, moves)
			KNIGHT:
				_knight_moves(square, colour, moves)
			BISHOP:
				_sliding_moves(square, colour, BISHOP_DIRS, moves)
			ROOK:
				_sliding_moves(square, colour, ROOK_DIRS, moves)
			QUEEN:
				_sliding_moves(square, colour, BISHOP_DIRS + ROOK_DIRS, moves)
			KING:
				_king_moves(square, colour, moves)
	return moves


func _pawn_moves(square: int, colour: int, moves: Array) -> void:
	var f := file_of(square)
	var r := rank_of(square)
	var dir: int = 1 if colour == WHITE else -1
	var start_rank: int = 1 if colour == WHITE else 6
	var promo_rank: int = 7 if colour == WHITE else 0
	var one_r: int = r + dir
	if _on(f, one_r):
		var one: int = sq(f, one_r)
		if board[one] == 0:
			if one_r == promo_rank:
				for p in PROMO_TYPES:
					moves.append(_mv(square, one, p, "promo"))
			else:
				moves.append(_mv(square, one, 0, "normal"))
			if r == start_rank:
				var two_r: int = r + dir * 2
				var two: int = sq(f, two_r)
				if board[two] == 0:
					moves.append(_mv(square, two, 0, "double"))
		for df in [-1, 1]:
			var nf: int = f + int(df)
			if not _on(nf, one_r):
				continue
			var target := sq(nf, one_r)
			var tp: int = board[target]
			if tp != 0 and piece_colour(tp) != colour and piece_type(tp) != KING:
				if one_r == promo_rank:
					for p in PROMO_TYPES:
						moves.append(_mv(square, target, p, "promo"))
				else:
					moves.append(_mv(square, target, 0, "normal"))
			elif target == ep_square:
				var cap_sq: int = target - 8 * dir
				if cap_sq >= 0 and cap_sq < 64:
					var cp: int = board[cap_sq]
					if cp != 0 and piece_colour(cp) != colour and piece_type(cp) == PAWN:
						moves.append(_mv(square, target, 0, "ep"))


func _knight_moves(square: int, colour: int, moves: Array) -> void:
	var f := file_of(square)
	var r := rank_of(square)
	for d in KNIGHT_DELTAS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		if _on(nf, nr):
			var target := sq(nf, nr)
			var tp: int = board[target]
			if tp == 0 or (piece_colour(tp) != colour and piece_type(tp) != KING):
				moves.append(_mv(square, target, 0, "normal"))


func _sliding_moves(square: int, colour: int, dirs: Array, moves: Array) -> void:
	var f := file_of(square)
	var r := rank_of(square)
	for d in dirs:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		while _on(nf, nr):
			var target := sq(nf, nr)
			var tp: int = board[target]
			if tp == 0:
				moves.append(_mv(square, target, 0, "normal"))
			else:
				if piece_colour(tp) != colour and piece_type(tp) != KING:
					moves.append(_mv(square, target, 0, "normal"))
				break
			nf += int(d[0])
			nr += int(d[1])


func _king_moves(square: int, colour: int, moves: Array) -> void:
	var f := file_of(square)
	var r := rank_of(square)
	for d in KING_DIRS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		if _on(nf, nr):
			var target := sq(nf, nr)
			var tp: int = board[target]
			if tp == 0 or (piece_colour(tp) != colour and piece_type(tp) != KING):
				moves.append(_mv(square, target, 0, "normal"))
	# castling
	if colour == WHITE and square == 4:
		if castling & CASTLE_WK and board[5] == 0 and board[6] == 0:
			if board[7] == make_piece(colour, ROOK) and not in_check(WHITE) \
					and not _attacked(5, BLACK) and not _attacked(6, BLACK):
				moves.append(_mv(4, 6, 0, "castle_k"))
		if castling & CASTLE_WQ and board[1] == 0 and board[2] == 0 and board[3] == 0:
			if board[0] == make_piece(colour, ROOK) and not in_check(WHITE) \
					and not _attacked(3, BLACK) and not _attacked(2, BLACK):
				moves.append(_mv(4, 2, 0, "castle_q"))
	elif colour == BLACK and square == 60:
		if castling & CASTLE_BK and board[61] == 0 and board[62] == 0:
			if board[63] == make_piece(colour, ROOK) and not in_check(BLACK) \
					and not _attacked(61, WHITE) and not _attacked(62, WHITE):
				moves.append(_mv(60, 62, 0, "castle_k"))
		if castling & CASTLE_BQ and board[57] == 0 and board[58] == 0 and board[59] == 0:
			if board[56] == make_piece(colour, ROOK) and not in_check(BLACK) \
					and not _attacked(59, WHITE) and not _attacked(58, WHITE):
				moves.append(_mv(60, 58, 0, "castle_q"))


func _mv(from_sq: int, to_sq: int, promo: int = 0, flag: String = "normal") -> Dictionary:
	return {"from": from_sq, "to": to_sq, "promo": promo, "flag": flag}


func _attacked(square: int, by_colour: int) -> bool:
	var f := file_of(square)
	var r := rank_of(square)
	# pawns
	var pawn_rank: int = r - 1 if by_colour == WHITE else r + 1
	for df in [-1, 1]:
		var pf: int = f + int(df)
		if _on(pf, pawn_rank):
			var p: int = board[sq(pf, pawn_rank)]
			if p == make_piece(by_colour, PAWN):
				return true
	# knights
	for d in KNIGHT_DELTAS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		if _on(nf, nr):
			var p: int = board[sq(nf, nr)]
			if p == make_piece(by_colour, KNIGHT):
				return true
	# king
	for d in KING_DIRS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		if _on(nf, nr):
			var p: int = board[sq(nf, nr)]
			if p == make_piece(by_colour, KING):
				return true
	# sliders
	for d in BISHOP_DIRS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		while _on(nf, nr):
			var p: int = board[sq(nf, nr)]
			if p != 0:
				if piece_colour(p) == by_colour and (piece_type(p) == BISHOP or piece_type(p) == QUEEN):
					return true
				break
			nf += int(d[0])
			nr += int(d[1])
	for d in ROOK_DIRS:
		var nf: int = f + int(d[0])
		var nr: int = r + int(d[1])
		while _on(nf, nr):
			var p: int = board[sq(nf, nr)]
			if p != 0:
				if piece_colour(p) == by_colour and (piece_type(p) == ROOK or piece_type(p) == QUEEN):
					return true
				break
			nf += int(d[0])
			nr += int(d[1])
	return false


func find_king(colour: int) -> int:
	var target := make_piece(colour, KING)
	for i in range(64):
		if board[i] == target:
			return i
	return -1


func in_check(colour: int) -> bool:
	var k := find_king(colour)
	if k < 0:
		return true
	return _attacked(k, opposite(colour))


# ----------------------------------------------------------------------------- apply move
func _apply_move(mv: Dictionary) -> Dictionary:
	var from_sq: int = mv["from"]
	var to_sq: int = mv["to"]
	var promo: int = mv.get("promo", 0)
	var flag: String = mv.get("flag", "normal")
	var piece: int = board[from_sq]
	var colour := piece_colour(piece)
	var type := piece_type(piece)

	var captured := 0
	var captured_sq := -1
	var rook_from := -1
	var rook_to := -1

	if flag == "ep":
		var dir: int = 1 if colour == WHITE else -1
		captured_sq = to_sq - 8 * dir
		if captured_sq >= 0 and captured_sq < 64:
			captured = board[captured_sq]
			board[captured_sq] = 0
	elif board[to_sq] != 0:
		captured = board[to_sq]
		captured_sq = to_sq

	if captured != 0 and piece_type(captured) == ROOK:
		_clear_rook_right(captured_sq)

	board[from_sq] = 0
	if promo != 0:
		board[to_sq] = make_piece(colour, promo)
	else:
		board[to_sq] = piece

	if type == KING:
		if colour == WHITE:
			castling &= ~(CASTLE_WK | CASTLE_WQ)
		else:
			castling &= ~(CASTLE_BK | CASTLE_BQ)
	elif type == ROOK:
		_clear_rook_right(from_sq)

	if flag == "castle_k":
		rook_from = from_sq + 3
		rook_to = from_sq + 1
		board[rook_to] = board[rook_from]
		board[rook_from] = 0
	elif flag == "castle_q":
		rook_from = from_sq - 4
		rook_to = from_sq - 1
		board[rook_to] = board[rook_from]
		board[rook_from] = 0

	if type == PAWN and abs(to_sq - from_sq) == 16:
		ep_square = int((from_sq + to_sq) / 2)
	else:
		ep_square = -1

	return {
		"from": from_sq, "to": to_sq, "piece": piece, "type": type,
		"colour": colour, "captured": captured, "captured_sq": captured_sq,
		"rook_from": rook_from, "rook_to": rook_to, "promo": promo,
		"flag": flag,
	}


func _clear_rook_right(square: int) -> void:
	match square:
		0: castling &= ~CASTLE_WQ
		7: castling &= ~CASTLE_WK
		56: castling &= ~CASTLE_BQ
		63: castling &= ~CASTLE_BK


func make_move(mv: Dictionary) -> Dictionary:
	var snapshot := _full_snapshot()
	var mover := turn
	var piece: int = board[mv["from"]]
	var type := piece_type(piece)
	var was_capture: bool = board[mv["to"]] != 0 or mv.get("flag", "") == "ep"
	var notation_base := _notation_base(mv, piece)
	var info := _apply_move(mv)
	info["mover"] = mover

	if type == PAWN or was_capture:
		halfmove = 0
	else:
		halfmove += 1
	if mover == BLACK:
		fullmove += 1

	last_move = {"from": mv["from"], "to": mv["to"]}
	if info["captured"] != 0:
		if mover == WHITE:
			captured_by_white.append(info["captured"])
		else:
			captured_by_black.append(info["captured"])

	turn = opposite(turn)
	_update_status()

	var suffix := ""
	if game_over and result_reason == "checkmate":
		suffix = "#"
	elif in_check(turn):
		suffix = "+"
	var notation := notation_base + suffix
	info["notation"] = notation
	move_log.append({
		"text": notation,
		"side": mover,
		"number": fullmove if mover == WHITE else fullmove - 1,
	})
	history.append(snapshot)
	return info


func make_search_move(mv: Dictionary) -> Dictionary:
	var mover := turn
	var piece: int = board[mv["from"]]
	var was_capture: bool = board[mv["to"]] != 0 or mv.get("flag", "") == "ep"
	var info := _apply_move(mv)
	if piece_type(piece) == PAWN or was_capture:
		halfmove = 0
	else:
		halfmove += 1
	if mover == BLACK:
		fullmove += 1
	last_move = {"from": mv["from"], "to": mv["to"]}
	turn = opposite(turn)
	return info


func undo() -> bool:
	if history.is_empty():
		return false
	_restore_full(history.pop_back())
	return true


func _notation_base(mv: Dictionary, piece: int) -> String:
	var flag: String = mv.get("flag", "normal")
	if flag == "castle_k":
		return "O-O"
	if flag == "castle_q":
		return "O-O-O"
	var type := piece_type(piece)
	var letters := ["", "", "N", "B", "R", "Q", "K"]
	var s: String = str(letters[type])
	var capture: bool = board[mv["to"]] != 0 or flag == "ep"
	s += _square_name(mv["from"])
	s += "x" if capture else "-"
	s += _square_name(mv["to"])
	if mv.get("promo", 0) != 0:
		var promo_letters := ["", "", "N", "B", "R", "Q"]
		s += "=" + promo_letters[mv["promo"]]
	return s


# ----------------------------------------------------------------------------- status
func _update_status() -> void:
	game_over = false
	result_text = ""
	result_reason = ""
	if halfmove >= 100:
		game_over = true
		result_text = "和棋"
		result_reason = "50回合规则"
		return
	if is_insufficient_material():
		game_over = true
		result_text = "和棋"
		result_reason = "子力不足"
		return
	var moves := legal_moves(turn)
	if moves.is_empty():
		game_over = true
		if in_check(turn):
			result_text = "白方胜利" if turn == BLACK else "黑方胜利"
			result_reason = "checkmate"
		else:
			result_text = "和棋"
			result_reason = "stalemate"


func is_insufficient_material() -> bool:
	var minors := 0
	var bishops := 0
	for i in range(64):
		var p: int = board[i]
		if p == 0:
			continue
		var t := piece_type(p)
		if t == PAWN or t == ROOK or t == QUEEN:
			return false
		if t == KNIGHT or t == BISHOP:
			minors += 1
			if t == BISHOP:
				bishops += 1
	if minors <= 1:
		return true
	return false


# ----------------------------------------------------------------------------- snapshots
func _min_snapshot() -> Dictionary:
	return {
		"board": board.duplicate(), "turn": turn,
		"castling": castling, "ep": ep_square,
	}


func _restore_min(snap: Dictionary) -> void:
	board = snap["board"].duplicate()
	turn = snap["turn"]
	castling = snap["castling"]
	ep_square = snap["ep"]


func _full_snapshot() -> Dictionary:
	return {
		"board": board.duplicate(),
		"turn": turn,
		"castling": castling,
		"ep": ep_square,
		"halfmove": halfmove,
		"fullmove": fullmove,
		"move_log": move_log.duplicate(true),
		"captured_w": captured_by_white.duplicate(),
		"captured_b": captured_by_black.duplicate(),
		"last_move": last_move.duplicate(true),
		"game_over": game_over,
		"result_text": result_text,
		"result_reason": result_reason,
	}


func _restore_full(snap: Dictionary) -> void:
	board = snap["board"].duplicate()
	turn = snap["turn"]
	castling = snap["castling"]
	ep_square = snap["ep"]
	halfmove = snap["halfmove"]
	fullmove = snap["fullmove"]
	move_log = snap["move_log"].duplicate(true)
	captured_by_white = snap["captured_w"].duplicate()
	captured_by_black = snap["captured_b"].duplicate()
	last_move = snap["last_move"].duplicate(true)
	game_over = snap["game_over"]
	result_text = snap["result_text"]
	result_reason = snap["result_reason"]


func clone() -> ChessGame:
	var g := ChessGame.new()
	g.board = board.duplicate()
	g.turn = turn
	g.castling = castling
	g.ep_square = ep_square
	g.halfmove = halfmove
	g.fullmove = fullmove
	g.last_move = last_move.duplicate(true)
	return g


# ----------------------------------------------------------------------------- perft (tests)
func perft(depth: int) -> int:
	if depth <= 0:
		return 1
	var moves := legal_moves()
	if depth == 1:
		return moves.size()
	var total := 0
	for mv in moves:
		var snap := _min_snapshot()
		_apply_move(mv)
		turn = opposite(turn)
		total += perft(depth - 1)
		turn = opposite(turn)
		_restore_min(snap)
	return total


