#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

struct GameState {
  uint32_t gameId;
  uint32_t player1Id;
  uint32_t player2Id;
  uint8_t boardSize;
  uint8_t *board;
  uint32_t currentTurn;
  uint32_t moveCount;

  // Time management
  uint16_t timeLimit;       // Time limit per player (0 = unlimited)
  uint16_t player1TimeLeft; // Remaining time for player 1
  uint16_t player2TimeLeft; // Remaining time for player 2
  std::chrono::steady_clock::time_point lastMoveTime;
  bool timerActive;

  // Draw offer
  bool drawOffered;
  uint32_t drawOfferedBy;

  // Rematch
  uint32_t lastGameWinner;

  GameState()
      : board(nullptr), moveCount(0), timeLimit(0), player1TimeLeft(0),
        player2TimeLeft(0), timerActive(false), drawOffered(false),
        drawOfferedBy(0), lastGameWinner(0) {}

  ~GameState() {
    if (board) {
      delete[] board;
      board = nullptr;
    }
  }
};

class GameLogic {
public:
  static bool isValidMove(GameState *game, uint8_t x, uint8_t y) {
    // Check bounds
    if (x >= game->boardSize || y >= game->boardSize) {
      return false;
    }

    // Check if cell is empty
    if (game->board[y * game->boardSize + x] != 0) {
      return false;
    }

    return true;
  }

  static bool checkWin(GameState *game, uint8_t x, uint8_t y, uint8_t player) {
    int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

    for (int d = 0; d < 4; d++) {
      int dx = directions[d][0];
      int dy = directions[d][1];
      int count = 1;

      // Check positive direction
      for (int i = 1; i < 5; i++) {
        int nx = x + dx * i;
        int ny = y + dy * i;

        if (nx < 0 || nx >= game->boardSize || ny < 0 ||
            ny >= game->boardSize) {
          break;
        }

        if (game->board[ny * game->boardSize + nx] == player) {
          count++;
        } else {
          break;
        }
      }

      // Check negative direction
      for (int i = 1; i < 5; i++) {
        int nx = x - dx * i;
        int ny = y - dy * i;

        if (nx < 0 || nx >= game->boardSize || ny < 0 ||
            ny >= game->boardSize) {
          break;
        }

        if (game->board[ny * game->boardSize + nx] == player) {
          count++;
        } else {
          break;
        }
      }

      if (count >= 5) {
        return true;
      }
    }

    return false;
  }

  static bool checkDraw(GameState *game) {
    // Check if board is full (no empty cells)
    int totalCells = game->boardSize * game->boardSize;
    for (int i = 0; i < totalCells; i++) {
      if (game->board[i] == 0) {
        return false;
      }
    }
    return true;
  }

  // Update time for current player after a move
  static void updateTimeAfterMove(GameState *game) {
    if (game->timeLimit == 0)
      return; // Unlimited time

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - game->lastMoveTime)
                       .count();

    if (game->currentTurn == game->player1Id) {
      game->player1TimeLeft =
          std::max(0, (int)game->player1TimeLeft - (int)elapsed);
    } else {
      game->player2TimeLeft =
          std::max(0, (int)game->player2TimeLeft - (int)elapsed);
    }

    game->lastMoveTime = now;
  }

  // Check if current player has timed out
  static bool checkTimeout(GameState *game) {
    if (game->timeLimit == 0)
      return false; // Unlimited time

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - game->lastMoveTime)
                       .count();

    if (game->currentTurn == game->player1Id) {
      return (int)game->player1TimeLeft - (int)elapsed <= 0;
    } else {
      return (int)game->player2TimeLeft - (int)elapsed <= 0;
    }
  }

  // Get remaining time for a player
  static uint16_t getRemainingTime(GameState *game, uint32_t playerId) {
    if (game->timeLimit == 0)
      return 0;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       now - game->lastMoveTime)
                       .count();

    if (playerId == game->player1Id) {
      if (game->currentTurn == game->player1Id) {
        return std::max(0, (int)game->player1TimeLeft - (int)elapsed);
      }
      return game->player1TimeLeft;
    } else {
      if (game->currentTurn == game->player2Id) {
        return std::max(0, (int)game->player2TimeLeft - (int)elapsed);
      }
      return game->player2TimeLeft;
    }
  }

  // Create a string representation of the board
  static std::string boardToString(GameState *game) {
    std::string result;
    result += "   ";
    for (int x = 0; x < game->boardSize; x++) {
      if (x < 10)
        result += " ";
      result += std::to_string(x) + " ";
    }
    result += "\n";

    for (int y = 0; y < game->boardSize; y++) {
      if (y < 10)
        result += " ";
      result += std::to_string(y) + " ";

      for (int x = 0; x < game->boardSize; x++) {
        uint8_t cell = game->board[y * game->boardSize + x];
        if (cell == 0) {
          result += " . ";
        } else if (cell == 1) {
          result += " X ";
        } else {
          result += " O ";
        }
      }
      result += "\n";
    }

    return result;
  }
};

#endif
