class_name ChessAI
extends RefCounted

const Game = preload("res://scripts/chess_game.gd")

const MATE := 1000000
const INF := 2000000

const VALUES := [0, 100, 320, 330, 500, 900, 20000]

# Piece-square tables are written from White's point of view,
# with rank 8 on the first row (as in a printed diagram).
const PST_PAWN := [
	0, 0, 0, 0, 0, 0, 0, 0,
	50, 50, 50, 50, 50, 50, 50, 50,
	10, 10, 20, 30, 30, 20, 10, 10,
	5, 5, 10, 25, 25, 10, 5, 5,
	0, 0, 0, 20, 20, 0, 0, 0,
	5, -5, -10, 0, 0, -10, -5, 5,
	5, 10, 10, -20, -20, 10, 10, 5,
	0, 0, 0, 0, 0, 0, 0, 0,
]

const PST_KNIGHT := [
	-50, -40, -30, -30, -30, -30, -40, -50,
	-40, -20, 0, 0, 0, 0, -20, -40,
	-30, 0, 10, 15, 15, 10, 0, -30,
	-30, 5, 15, 20, 20, 15, 5, -30,
	-30, 0, 15, 20, 20, 15, 0, -30,
	-30, 5, 10, 15, 15, 10, 5, -30,
	-40, -20, 0, 5, 5, 0, -20, -40,
	-50, -40, -30, -30, -30, -30, -40, -50,
]

const PST_BISHOP := [
	-20, -10, -10, -10, -10, -10, -10, -20,
	-10, 0, 0, 0, 0, 0, 0, -10,
	-10, 0, 5, 10, 10, 5, 0, -10,
	-10, 5, 5, 10, 10, 5, 5, -10,
	-10, 0, 10, 10, 10, 10, 0, -10,
	-10, 10, 10, 10, 10, 10, 10, -10,
	-10, 5, 0, 0, 0, 0, 5, -10,
	-20, -10, -10, -10, -10, -10, -10, -20,
]

const PST_ROOK := [
	0, 0, 0, 0, 0, 0, 0, 0,
	5, 10, 10, 10, 10, 10, 10, 5,
	-5, 0, 0, 0, 0, 0, 0, -5,
	-5, 0, 0, 0, 0, 0, 0, -5,
	-5, 0, 0, 0, 0, 0, 0, -5,
	-5, 0, 0, 0, 0, 0, 0, -5,
	-5, 0, 0, 0, 0, 0, 0, -5,
	0, 0, 0, 5, 5, 0, 0, 0,
]

const PST_QUEEN := [
	-20, -10, -10, -5, -5, -10, -10, -20,
	-10, 0, 0, 0, 0, 0, 0, -10,
	-10, 0, 5, 5, 5, 5, 0, -10,
	-5, 0, 5, 5, 5, 5, 0, -5,
	0, 0, 5, 5, 5, 5, 0, -5,
	-10, 5, 5, 5, 5, 5, 0, -10,
	-10, 0, 5, 0, 0, 0, 0, -10,
	-20, -10, -10, -5, -5, -10, -10, -20,
]

const PST_KING := [
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-20, -30, -30, -40, -40, -30, -30, -20,
	-10, -20, -20, -20, -20, -20, -20, -10,
	20, 20, 0, 0, 0, 0, 20, 20,
	20, 30, 10, 0, 0, 10, 30, 20,
]

var nodes := 0
var root_depth := 3
var rng := RandomNumberGenerator.new()


func _init() -> void:
	rng.randomize()


func find_best_move(game: Game, depth: int = 3) -> Dictionary:
	root_depth = depth
	nodes = 0
	var moves: Array = game.legal_moves()
	if moves.is_empty():
		return {}
	moves.shuffle()
	moves = _order_moves(game, moves)

	var best_score := -INF
	var best_move: Dictionary = {}
	var alpha := -INF
	for mv in moves:
		var child: Game = game.clone()
		child.make_search_move(mv)
		var score := -_negamax(child, depth - 1, -INF, -alpha, 1)
		if score > best_score:
			best_score = score
			best_move = mv
		if best_score > alpha:
			alpha = best_score
	return best_move


func _negamax(game: Game, depth: int, alpha: int, beta: int, ply: int) -> int:
	nodes += 1
	if depth <= 0:
		return _eval_side(game)
	var moves: Array = game.legal_moves()
	if moves.is_empty():
		if game.in_check(game.turn):
			return -MATE + ply
		return 0
	moves = _order_moves(game, moves)
	var best := -INF
	for mv in moves:
		var child: Game = game.clone()
		child.make_search_move(mv)
		var score := -_negamax(child, depth - 1, -beta, -alpha, ply + 1)
		if score > best:
			best = score
		if best > alpha:
			alpha = best
		if alpha >= beta:
			break
	return best


func _eval_side(game: Game) -> int:
	var score := _evaluate_white(game)
	return score if game.turn == Game.WHITE else -score


func _evaluate_white(game: Game) -> int:
	var score := 0
	var white_bishops := 0
	var black_bishops := 0
	for square in range(64):
		var piece: int = game.board[square]
		if piece == 0:
			continue
		var colour: int = piece >> 3
		var type: int = piece & 7
		var table_index: int
		if colour == Game.WHITE:
			table_index = (7 - (square >> 3)) * 8 + (square & 7)
		else:
			table_index = (square >> 3) * 8 + (square & 7)
		var value: int = VALUES[type] + _pst(type, table_index)
		if colour == Game.WHITE:
			score += value
			if type == Game.BISHOP:
				white_bishops += 1
		else:
			score -= value
			if type == Game.BISHOP:
				black_bishops += 1
	if white_bishops >= 2:
		score += 30
	if black_bishops >= 2:
		score -= 30
	return score


func _pst(type: int, index: int) -> int:
	match type:
		Game.PAWN: return int(PST_PAWN[index])
		Game.KNIGHT: return int(PST_KNIGHT[index])
		Game.BISHOP: return int(PST_BISHOP[index])
		Game.ROOK: return int(PST_ROOK[index])
		Game.QUEEN: return int(PST_QUEEN[index])
		Game.KING: return int(PST_KING[index])
	return 0


func _order_moves(game: Game, moves: Array) -> Array:
	var scored: Array = []
	for mv in moves:
		scored.append([_move_score(game, mv), mv])
	scored.sort_custom(func(a, b): return a[0] > b[0])
	var result: Array = []
	for item in scored:
		result.append(item[1])
	return result


func _move_score(game: Game, mv: Dictionary) -> int:
	var score := 0
	var from_sq: int = mv["from"]
	var to_sq: int = mv["to"]
	var piece: int = game.board[from_sq]
	var type: int = piece & 7
	var target: int = game.board[to_sq]
	if target != 0:
		score += 10000 + VALUES[target & 7] * 10 - VALUES[type]
	elif mv.get("flag", "") == "ep":
		score += 10000 + VALUES[Game.PAWN] * 10 - VALUES[type]
	if mv.get("promo", 0) != 0:
		score += 8000 + VALUES[mv["promo"]]
	if mv.get("flag", "") in ["castle_k", "castle_q"]:
		score += 300
	# central preference as a light tie-breaker
	var file: int = to_sq & 7
	var rank: int = to_sq >> 3
	score += 4 - abs(file - 3) - abs(rank - 3)
	return score
