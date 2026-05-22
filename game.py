#!/usr/bin/env python3
"""UCI game between pawnstar-go and pawnstar-rs.

Time control: 5 minutes per 40 moves (standard).
"""

import subprocess
import threading
import sys
import time

REF_BIN  = "/home/jonny/work/pawnstar/build/pawnstar-ref"
TST_BIN  = "/home/jonny/work/pawnstar/build/pawnstar"

BASE_TIME_MS  = 1 * 60 * 1000   # 1 minute
MOVES_PER_TC  = 40


class Engine:
    def __init__(self, name: str, path: str):
        self.name = name
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._lock = threading.Lock()
        self._lines: list[str] = []
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            with self._lock:
                self._lines.append(line)

    def send(self, cmd: str):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def wait_for(self, token: str, timeout: float = 10.0) -> list[str]:
        """Return all lines up to and including the line containing token."""
        deadline = time.time() + timeout
        collected: list[str] = []
        while time.time() < deadline:
            with self._lock:
                while self._lines:
                    line = self._lines.pop(0)
                    collected.append(line)
                    if token in line:
                        return collected
            time.sleep(0.01)
        return collected

    def drain(self) -> list[str]:
        with self._lock:
            lines, self._lines = self._lines, []
        return lines

    def quit(self):
        try:
            self.send("quit")
            self.proc.wait(timeout=2)
        except Exception:
            self.proc.kill()


def normalize_move(bm: str) -> str:
    """Normalize promotion notation: e7e8=Q → e7e8q (UCI standard)."""
    if len(bm) == 6 and bm[4] == '=':
        return bm[:4] + bm[5].lower()
    return bm


def uci_handshake(engine: Engine):
    engine.send("uci")
    engine.wait_for("uciok", timeout=5)
    engine.send("isready")
    engine.wait_for("readyok", timeout=5)
    engine.send("ucinewgame")
    engine.send("isready")
    engine.wait_for("readyok", timeout=5)


def do_move(mover: Engine, other: Engine, moves: list[str],
            mover_time_ms: int, other_time_ms: int,
            moves_remaining: int, mover_is_white: bool) -> str:
    """Send position + go to mover, stream info lines, return bestmove."""
    pos_cmd = "position startpos"
    if moves:
        pos_cmd += " moves " + " ".join(moves)
    mover.send(pos_cmd)

    wtime = mover_time_ms if mover_is_white else other_time_ms
    btime = other_time_ms if mover_is_white else mover_time_ms
    go_cmd = (
        f"go wtime {wtime} btime {btime} movestogo {moves_remaining}"
    )
    mover.send(go_cmd)

    # Stream all engine output until bestmove
    deadline = time.time() + (mover_time_ms / 1000) + 60  # generous safety margin
    while time.time() < deadline:
        for line in mover.drain():
            print(f"  [{mover.name}] {line}", flush=True)
            if line.startswith("bestmove"):
                parts = line.split()
                return parts[1] if len(parts) > 1 else "0000"
        time.sleep(0.01)

    print(f"  [{mover.name}] TIMEOUT waiting for bestmove")
    mover.send("stop")
    for line in mover.wait_for("bestmove", timeout=5):
        print(f"  [{mover.name}] {line}", flush=True)
        if line.startswith("bestmove"):
            parts = line.split()
            return parts[1] if len(parts) > 1 else "0000"
    return "0000"


def run_game():
    print("Starting engines...")
    white = Engine("pawnstar-test white", TST_BIN)
    black = Engine("pawnstar-ref  black", REF_BIN)

    uci_handshake(white)
    uci_handshake(black)

    moves: list[str] = []
    white_time_ms = BASE_TIME_MS
    black_time_ms = BASE_TIME_MS
    move_number = 1
    period_moves_played = 0   # within the current 40-move period

    print(f"\n{'='*60}")
    print(f"  pawnstar-test (White) vs pawnstar-ref (Black)")
    print(f"  Time control: {BASE_TIME_MS//60000} min / {MOVES_PER_TC} moves")
    print(f"{'='*60}\n")

    while True:
        moves_remaining = MOVES_PER_TC - period_moves_played

        # White's move
        print(f"Move {move_number} (White) — {white_time_ms/1000:.1f}s remaining")
        t0 = time.time()
        bm = do_move(white, black, moves, white_time_ms, black_time_ms,
                     moves_remaining, mover_is_white=True)
        white_time_ms -= int((time.time() - t0) * 1000)
        if bm == "0000":
            print("Game over — White has no legal moves (checkmate or stalemate).")
            break
        moves.append(normalize_move(bm))
        period_moves_played += 1

        # Reset time if period complete
        if period_moves_played == MOVES_PER_TC:
            white_time_ms = BASE_TIME_MS
            black_time_ms = BASE_TIME_MS
            period_moves_played = 0
        moves_remaining = MOVES_PER_TC - period_moves_played

        # Black's move
        print(f"Move {move_number} (Black) — {black_time_ms/1000:.1f}s remaining")
        t0 = time.time()
        bm = do_move(black, white, moves, black_time_ms, white_time_ms,
                     moves_remaining, mover_is_white=False)
        black_time_ms -= int((time.time() - t0) * 1000)
        if bm == "0000":
            print("Game over — Black has no legal moves (checkmate or stalemate).")
            break
        moves.append(normalize_move(bm))
        period_moves_played += 1

        if period_moves_played == MOVES_PER_TC:
            white_time_ms = BASE_TIME_MS
            black_time_ms = BASE_TIME_MS
            period_moves_played = 0

        move_number += 1

        if move_number > 200:
            print("Draw by move limit (200 moves).")
            break

    print("\nMove list:", " ".join(moves))
    white.quit()
    black.quit()


if __name__ == "__main__":
    run_game()
