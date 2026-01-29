#include "../cpp-server/protocol.h"
#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// ANSI Color codes for terminal UI
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define BG_BLACK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"

class GomokuClient {
private:
  int clientSocket;
  uint32_t userId;
  uint32_t sessionId;
  std::atomic<bool> connected;
  std::atomic<bool> inGame;
  std::atomic<bool> isMyTurn;
  uint32_t currentGameId;
  uint32_t opponentId;
  uint8_t currentBoardSize;
  uint8_t *gameBoard;
  std::string myUsername;
  std::string opponentName;
  bool isPlayer1;

  // Stats
  uint16_t eloRating;
  uint16_t wins;
  uint16_t losses;
  uint16_t draws;

  // Helper to clear input buffer
  void clearInputBuffer() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }

  // Safe integer input
  int getIntInput() {
    int value;
    while (!(std::cin >> value)) {
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << RED << "Invalid input. Enter a number: " << RESET;
    }
    return value;
  }

public:
  GomokuClient()
      : userId(0), sessionId(0), connected(false), inGame(false),
        isMyTurn(false), currentGameId(0), gameBoard(nullptr), isPlayer1(true),
        eloRating(0), wins(0), losses(0), draws(0) {}

  void clearScreen() { std::cout << "\033[2J\033[1;1H"; }

  void printHeader() {
    std::cout << CYAN << BOLD;
    std::cout << "╔══════════════════════════════════════════════════════════╗"
              << std::endl;
    std::cout << "║           🎮 GOMOKU - LAN MULTIPLAYER 🎮                 ║"
              << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝"
              << RESET << std::endl;
  }

  void printUserInfo() {
    if (userId > 0) {
      std::cout << GREEN << "┌─ " << BOLD << myUsername << RESET << GREEN
                << " ─ ELO: " << YELLOW << eloRating << GREEN
                << " │ W: " << wins << " L: " << losses << " D: " << draws
                << " ─┐" << RESET << std::endl;
    }
  }

  bool connectToServer(const char *host, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
      std::cerr << RED << "Error creating socket" << RESET << std::endl;
      return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host, &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) < 0) {
      std::cerr << RED << "Error connecting to server at " << host << ":"
                << port << RESET << std::endl;
      return false;
    }

    connected = true;
    std::cout << GREEN << "✓ Connected to server at " << host << ":" << port
              << RESET << std::endl;

    // Start receive thread
    std::thread(&GomokuClient::receiveMessages, this).detach();

    return true;
  }

  // ==================== AUTHENTICATION ====================

  void registerUser() {
    RegisterRequest req;
    memset(&req, 0, sizeof(req));

    clearInputBuffer();
    std::cout << CYAN << "┌─── REGISTER ───┐" << RESET << std::endl;
    std::cout << "│ Username: ";
    std::cin >> req.username;
    std::cout << "│ Email: ";
    std::cin >> req.email;
    std::cout << "│ Password: ";
    std::cin >> req.password;
    std::cout << CYAN << "└────────────────┘" << RESET << std::endl;

    sendMessage(MSG_REGISTER, &req, sizeof(req));
  }

  void login() {
    LoginRequest req;
    memset(&req, 0, sizeof(req));

    clearInputBuffer();
    std::cout << CYAN << "┌─── LOGIN ───┐" << RESET << std::endl;
    std::cout << "│ Username: ";
    std::cin >> req.username;
    std::cout << "│ Password: ";
    std::cin >> req.password;
    std::cout << CYAN << "└─────────────┘" << RESET << std::endl;

    myUsername = req.username;
    sendMessage(MSG_LOGIN, &req, sizeof(req));
  }

  // ==================== PLAYER LIST ====================

  void getOnlinePlayers() { sendMessage(MSG_GET_ONLINE_PLAYERS, nullptr, 0); }

  // ==================== CHALLENGE ====================

  void sendChallenge() {
    ChallengeRequest req;
    memset(&req, 0, sizeof(req));

    std::cout << CYAN << "┌─── SEND CHALLENGE ───┐" << RESET << std::endl;
    std::cout << "│ Target User ID: ";
    req.targetUserId = getIntInput();
    std::cout << "│ Board Size (10-19, default 15): ";
    int boardSize = getIntInput();
    req.boardSize = (boardSize >= 10 && boardSize <= 19) ? boardSize : 15;
    std::cout << "│ Time Limit in seconds (0=unlimited): ";
    req.timeLimit = getIntInput();
    std::cout << CYAN << "└──────────────────────┘" << RESET << std::endl;

    sendMessage(MSG_SEND_CHALLENGE, &req, sizeof(req));
  }

  void acceptChallenge(uint32_t challengeId) {
    sendMessage(MSG_ACCEPT_CHALLENGE, &challengeId, sizeof(challengeId));
  }

  void declineChallenge(uint32_t challengeId) {
    sendMessage(MSG_DECLINE_CHALLENGE, &challengeId, sizeof(challengeId));
  }

  // ==================== GAME PLAY ====================

  void makeMove() {
    if (!inGame) {
      std::cout << RED << "You are not in a game!" << RESET << std::endl;
      return;
    }

    if (!isMyTurn) {
      std::cout << RED << "It's not your turn! Wait for opponent." << RESET
                << std::endl;
      return;
    }

    MoveRequest req;
    req.gameId = currentGameId;

    std::cout << GREEN << "Enter move (x y): " << RESET;
    int x = getIntInput();
    int y = getIntInput();
    req.x = x;
    req.y = y;

    sendMessage(MSG_MAKE_MOVE, &req, sizeof(req));
  }

  void resign() {
    if (!inGame) {
      std::cout << RED << "You are not in a game!" << RESET << std::endl;
      return;
    }

    std::cout << YELLOW << "Are you sure you want to resign? (y/n): " << RESET;
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y') {
      std::cout << "Resignation cancelled." << std::endl;
      return;
    }

    ResignRequest req;
    req.gameId = currentGameId;
    sendMessage(MSG_RESIGN, &req, sizeof(req));

    std::cout << YELLOW << "You resigned from the game." << RESET << std::endl;
  }

  void offerDraw() {
    if (!inGame) {
      std::cout << RED << "You are not in a game!" << RESET << std::endl;
      return;
    }

    DrawRequest req;
    req.gameId = currentGameId;
    sendMessage(MSG_OFFER_DRAW, &req, sizeof(req));

    std::cout << YELLOW << "Draw offer sent to opponent." << RESET << std::endl;
  }

  void acceptDraw() {
    DrawRequest req;
    req.gameId = currentGameId;
    sendMessage(MSG_ACCEPT_DRAW, &req, sizeof(req));
  }

  void declineDraw() {
    DrawRequest req;
    req.gameId = currentGameId;
    sendMessage(MSG_DECLINE_DRAW, &req, sizeof(req));
    std::cout << "Draw declined." << std::endl;
  }

  void requestRematch() {
    RematchRequest req;
    req.lastGameId = currentGameId;
    req.opponentId = opponentId;
    sendMessage(MSG_REQUEST_REMATCH, &req, sizeof(req));

    std::cout << YELLOW << "Rematch request sent!" << RESET << std::endl;
  }

  void acceptRematch(uint32_t gameId) {
    sendMessage(MSG_ACCEPT_REMATCH, &gameId, sizeof(gameId));
  }

  void declineRematch(uint32_t gameId) {
    sendMessage(MSG_DECLINE_REMATCH, &gameId, sizeof(gameId));
  }

  // ==================== GAME HISTORY ====================

  void getGameHistory() { sendMessage(MSG_GET_GAME_HISTORY, nullptr, 0); }

  void getGameLog() {
    std::cout << "Enter Game ID: ";
    uint32_t gameId = getIntInput();
    sendMessage(MSG_GET_GAME_LOG, &gameId, sizeof(gameId));
  }

  // ==================== BOARD DISPLAY ====================

  void displayBoard() {
    if (!gameBoard || !inGame)
      return;

    std::cout << std::endl;
    std::cout << BOLD << "  ╔════════════════════════════════════════════════╗"
              << RESET << std::endl;
    std::cout << BOLD << "  ║  " << CYAN << "GAME #" << currentGameId << RESET
              << BOLD << "  │  " << (isPlayer1 ? GREEN : RED) << myUsername
              << RESET << BOLD << " vs " << (!isPlayer1 ? GREEN : RED)
              << opponentName << RESET << BOLD << "  ║" << RESET << std::endl;
    std::cout << BOLD << "  ╚════════════════════════════════════════════════╝"
              << RESET << std::endl;

    // Legend
    std::cout << "  " << GREEN << "X" << RESET << " = "
              << (isPlayer1 ? "You" : "Opponent") << "  │  " << RED << "O"
              << RESET << " = " << (isPlayer1 ? "Opponent" : "You")
              << std::endl;

    // Column headers
    std::cout << "      ";
    for (int x = 0; x < currentBoardSize; x++) {
      if (x < 10)
        std::cout << " ";
      std::cout << CYAN << x << RESET << " ";
    }
    std::cout << std::endl;

    // Top border
    std::cout << "     ╔";
    for (int x = 0; x < currentBoardSize; x++) {
      std::cout << "═══";
    }
    std::cout << "╗" << std::endl;

    // Board rows
    for (int y = 0; y < currentBoardSize; y++) {
      if (y < 10)
        std::cout << " ";
      std::cout << CYAN << y << RESET << "   ║";

      for (int x = 0; x < currentBoardSize; x++) {
        uint8_t cell = gameBoard[y * currentBoardSize + x];
        if (cell == 0) {
          std::cout << DIM << " · " << RESET;
        } else if (cell == 1) {
          std::cout << BOLD << GREEN << " X " << RESET;
        } else {
          std::cout << BOLD << RED << " O " << RESET;
        }
      }
      std::cout << "║" << std::endl;
    }

    // Bottom border
    std::cout << "     ╚";
    for (int x = 0; x < currentBoardSize; x++) {
      std::cout << "═══";
    }
    std::cout << "╝" << std::endl;

    // Turn indicator
    std::cout << std::endl;
    if (isMyTurn) {
      std::cout << GREEN << BOLD << ">>> YOUR TURN!" << RESET << std::endl;
    } else {
      std::cout << YELLOW << "Waiting for opponent..." << RESET << std::endl;
    }
  }

  // ==================== MESSAGE HANDLING ====================

  void sendMessage(uint16_t type, void *payload, uint32_t length) {
    MessageHeader header;
    header.type = type;
    header.length = length;
    header.userId = userId;
    header.sessionId = sessionId;

    send(clientSocket, &header, sizeof(header), 0);
    if (length > 0 && payload) {
      send(clientSocket, payload, length, 0);
    }
  }

  void receiveMessages() {
    while (connected) {
      MessageHeader header;
      int bytesRead = recv(clientSocket, &header, sizeof(header), 0);

      if (bytesRead <= 0) {
        std::cout << RED << "\n[!] Disconnected from server" << RESET
                  << std::endl;
        connected = false;
        break;
      }

      char *payload = nullptr;
      if (header.length > 0) {
        payload = new char[header.length];
        bytesRead = recv(clientSocket, payload, header.length, 0);

        if (bytesRead <= 0) {
          delete[] payload;
          break;
        }
      }

      handleMessage(header, payload);
      if (payload)
        delete[] payload;
    }
  }

  void handleMessage(MessageHeader &header, char *payload) {
    switch (header.type) {
    case MSG_LOGIN_RESPONSE: {
      LoginResponse *resp = (LoginResponse *)payload;
      if (resp->success) {
        userId = resp->userId;
        sessionId = resp->sessionId;
        eloRating = resp->eloRating;
        wins = resp->wins;
        losses = resp->losses;
        draws = resp->draws;

        std::cout << std::endl;
        std::cout << GREEN << "╔═══════════════════════════════════════╗"
                  << std::endl;
        std::cout << "║  ✓ LOGIN SUCCESSFUL                   ║" << std::endl;
        std::cout << "╠═══════════════════════════════════════╣" << std::endl;
        std::cout << "║  User ID: " << std::setw(6) << userId
                  << "                     ║" << std::endl;
        std::cout << "║  ELO Rating: " << std::setw(4) << eloRating
                  << "                   ║" << std::endl;
        std::cout << "║  Record: " << wins << "W / " << losses << "L / "
                  << draws << "D            ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════╝" << RESET
                  << std::endl;
      } else {
        std::cout << RED << "\n✗ Login failed: " << resp->message << RESET
                  << std::endl;
      }
      break;
    }

    case MSG_REGISTER_RESPONSE: {
      LoginResponse *resp = (LoginResponse *)payload;
      if (resp->success) {
        std::cout << GREEN << "\n✓ Registration successful! Please login."
                  << RESET << std::endl;
      } else {
        std::cout << RED << "\n✗ Registration failed: " << resp->message
                  << RESET << std::endl;
      }
      break;
    }

    case MSG_ONLINE_PLAYERS_LIST: {
      uint32_t count = *(uint32_t *)payload;

      std::cout << std::endl;
      std::cout << CYAN
                << "╔═══════════════════════════════════════════════════════╗"
                << std::endl;
      std::cout << "║              ONLINE PLAYERS (" << count
                << ")                        ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << std::endl;
      std::cout << "║  ID   │ Username          │  ELO  │ W/L/D   │ Status ║"
                << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << RESET << std::endl;

      for (uint32_t i = 0; i < count; i++) {
        PlayerInfo info;
        recv(clientSocket, &info, sizeof(info), 0);

        std::cout << CYAN << "║ " << RESET;
        std::cout << std::setw(5) << info.userId << " │ ";
        std::cout << std::setw(17) << std::left << info.username << std::right
                  << " │ ";
        std::cout << std::setw(5) << info.eloRating << " │ ";
        std::cout << std::setw(2) << info.wins << "/" << std::setw(2)
                  << info.losses << "/" << std::setw(2) << info.draws << " │ ";

        if (info.inGame) {
          std::cout << YELLOW << "In Game" << RESET;
        } else {
          std::cout << GREEN << " Ready " << RESET;
        }
        std::cout << CYAN << " ║" << RESET << std::endl;
      }

      std::cout << CYAN
                << "╚═══════════════════════════════════════════════════════╝"
                << RESET << std::endl;
      break;
    }

    case MSG_CHALLENGE_RECEIVED: {
      ChallengeResponse *resp = (ChallengeResponse *)payload;

      std::cout << std::endl;
      std::cout << YELLOW << BOLD << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║       ⚔️  CHALLENGE RECEIVED!  ⚔️       ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << RESET
                << YELLOW << std::endl;
      std::cout << "║  From: " << resp->challengerName << std::endl;
      std::cout << "║  Challenge ID: " << resp->challengeId << std::endl;
      std::cout << "║  Board: " << (int)resp->boardSize << "x"
                << (int)resp->boardSize << std::endl;
      std::cout << "║  Time: "
                << (resp->timeLimit > 0 ? std::to_string(resp->timeLimit) + "s"
                                        : "Unlimited")
                << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << std::endl;
      std::cout << "║  Use option 5 to Accept               ║" << std::endl;
      std::cout << "║  Use option 6 to Decline              ║" << std::endl;
      std::cout << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      break;
    }

    case MSG_CHALLENGE_DECLINED: {
      ChallengeDeclinedResponse *resp = (ChallengeDeclinedResponse *)payload;
      std::cout << std::endl;
      std::cout << RED << "[!] Challenge declined by " << resp->declinerName
                << RESET << std::endl;
      break;
    }

    case MSG_CHALLENGE_RESPONSE: {
      uint32_t challengeId = *(uint32_t *)payload;
      std::cout << GREEN << "\n✓ Challenge sent! (ID: " << challengeId
                << ") Waiting for response..." << RESET << std::endl;
      break;
    }

    case MSG_GAME_START: {
      GameStart *start = (GameStart *)payload;

      inGame = true;
      currentGameId = start->gameId;
      currentBoardSize = start->boardSize;
      isPlayer1 = (start->player1Id == userId);
      opponentId = isPlayer1 ? start->player2Id : start->player1Id;
      opponentName = isPlayer1 ? start->player2Name : start->player1Name;
      isMyTurn = (start->currentTurn == userId);

      // Initialize board
      if (gameBoard)
        delete[] gameBoard;
      gameBoard = new uint8_t[currentBoardSize * currentBoardSize]();

      clearScreen();
      printHeader();

      std::cout << std::endl;
      std::cout << GREEN << BOLD << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║          🎮 GAME STARTED! 🎮          ║" << std::endl;
      std::cout << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      std::cout << "You are: "
                << (isPlayer1 ? GREEN "X (first move)" : RED "O (second move)")
                << RESET << std::endl;

      displayBoard();
      break;
    }

    case MSG_MOVE_RESPONSE:
    case MSG_OPPONENT_MOVE: {
      MoveResponse *resp = (MoveResponse *)payload;

      if (gameBoard) {
        gameBoard[resp->y * currentBoardSize + resp->x] = resp->player;
      }
      isMyTurn = (resp->nextTurn == userId);

      clearScreen();
      printHeader();

      std::cout << MAGENTA << "\n[Move #" << resp->moveNumber << "] Player "
                << (resp->player == 1 ? "X" : "O") << " placed at ("
                << (int)resp->x << ", " << (int)resp->y << ")" << RESET
                << std::endl;

      displayBoard();
      break;
    }

    case MSG_DRAW_RECEIVED: {
      std::cout << std::endl;
      std::cout << YELLOW << BOLD << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║      🤝 DRAW OFFER RECEIVED! 🤝       ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << RESET
                << YELLOW << std::endl;
      std::cout << "║  Your opponent offers a draw.         ║" << std::endl;
      std::cout << "║  Use in-game menu to accept/decline   ║" << std::endl;
      std::cout << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      break;
    }

    case MSG_DECLINE_DRAW: {
      std::cout << std::endl;
      std::cout << RED << "[!] Draw offer declined by opponent." << RESET
                << std::endl;
      break;
    }

    case MSG_REMATCH_RECEIVED: {
      RematchRequest *req = (RematchRequest *)payload;
      currentGameId = req->lastGameId;

      std::cout << std::endl;
      std::cout << YELLOW << BOLD << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║      🔄 REMATCH REQUEST! 🔄           ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << RESET
                << YELLOW << std::endl;
      std::cout << "║  Opponent wants a rematch!            ║" << std::endl;
      std::cout << "║  Use option 12 to accept              ║" << std::endl;
      std::cout << "║  Use option 13 to decline             ║" << std::endl;
      std::cout << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      break;
    }

    case MSG_REMATCH_DECLINED: {
      std::cout << std::endl;
      std::cout << RED << "[!] Rematch declined by opponent." << RESET
                << std::endl;
      break;
    }

    case MSG_GAME_OVER: {
      GameOver *gameOver = (GameOver *)payload;

      inGame = false;
      isMyTurn = false;

      clearScreen();
      printHeader();
      displayBoard();

      std::cout << std::endl;
      if (gameOver->reason == 3) { // Draw
        std::cout << YELLOW << BOLD
                  << "╔═══════════════════════════════════════╗" << std::endl;
        std::cout << "║            🤝 GAME DRAW! 🤝           ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════╝" << RESET
                  << std::endl;
        draws++;
      } else if (gameOver->winnerId == userId) {
        std::cout << GREEN << BOLD
                  << "╔═══════════════════════════════════════╗" << std::endl;
        std::cout << "║        🏆 YOU WIN! 🏆                 ║" << std::endl;
        std::cout << "╠═══════════════════════════════════════╣" << RESET
                  << GREEN << std::endl;
        std::cout << "║  ELO Change: +" << gameOver->eloChange << std::endl;
        eloRating += gameOver->eloChange;
        wins++;
      } else {
        std::cout << RED << BOLD << "╔═══════════════════════════════════════╗"
                  << std::endl;
        std::cout << "║        😢 YOU LOSE! 😢                ║" << std::endl;
        std::cout << "╠═══════════════════════════════════════╣" << RESET << RED
                  << std::endl;
        std::cout << "║  ELO Change: -" << gameOver->eloChange << std::endl;
        eloRating -= gameOver->eloChange;
        losses++;
      }

      const char *reasonStr[] = {"Five in a row", "Resignation", "Timeout",
                                 "Draw agreed"};
      std::cout << "║  Reason: " << reasonStr[gameOver->reason] << std::endl;
      std::cout << "║  Total Moves: " << gameOver->totalMoves << std::endl;
      std::cout << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      std::cout << std::endl << "Use option 11 to request rematch" << std::endl;
      break;
    }

    case MSG_GAME_HISTORY_RESPONSE: {
      uint32_t count = *(uint32_t *)payload;

      std::cout << std::endl;
      std::cout << CYAN
                << "╔═══════════════════════════════════════════════════════╗"
                << std::endl;
      std::cout << "║              GAME HISTORY (" << count
                << " games)                    ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << std::endl;
      std::cout << "║  ID   │ Opponent          │ Result │  ELO  │ Date     ║"
                << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << RESET << std::endl;

      for (uint32_t i = 0; i < count; i++) {
        GameHistoryEntry entry;
        recv(clientSocket, &entry, sizeof(entry), 0);

        std::cout << CYAN << "║ " << RESET;
        std::cout << std::setw(5) << entry.gameId << " │ ";
        std::cout << std::setw(17) << std::left << entry.opponentName
                  << std::right << " │ ";

        if (entry.result == 0) {
          std::cout << GREEN << " WIN  " << RESET;
        } else if (entry.result == 1) {
          std::cout << RED << " LOSS " << RESET;
        } else {
          std::cout << YELLOW << " DRAW " << RESET;
        }

        std::cout << " │ ";
        if (entry.eloChange >= 0) {
          std::cout << GREEN << "+" << std::setw(4) << entry.eloChange << RESET;
        } else {
          std::cout << RED << std::setw(5) << entry.eloChange << RESET;
        }

        // Format timestamp
        time_t t = entry.timestamp;
        struct tm *tm_info = localtime(&t);
        char dateStr[11];
        strftime(dateStr, 11, "%Y-%m-%d", tm_info);
        std::cout << " │ " << dateStr;
        std::cout << CYAN << " ║" << RESET << std::endl;
      }

      std::cout << CYAN
                << "╚═══════════════════════════════════════════════════════╝"
                << RESET << std::endl;
      break;
    }

    case MSG_GAME_LOG_RESPONSE: {
      GameLogHeader *logHeader = (GameLogHeader *)payload;

      std::cout << std::endl;
      std::cout << CYAN
                << "╔═══════════════════════════════════════════════════════╗"
                << std::endl;
      std::cout << "║              GAME LOG #" << logHeader->gameId
                << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << std::endl;
      std::cout << "║  " << logHeader->player1Name << " (X) vs "
                << logHeader->player2Name << " (O)" << std::endl;
      std::cout << "║  Board: " << (int)logHeader->boardSize << "x"
                << (int)logHeader->boardSize
                << " │ Moves: " << logHeader->totalMoves
                << " │ Duration: " << logHeader->gameDuration << "s"
                << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << std::endl;
      std::cout << "║  #   │ Player │ Position │ Time (s)" << std::endl;
      std::cout << "╠═══════════════════════════════════════════════════════╣"
                << RESET << std::endl;

      for (uint32_t i = 0; i < logHeader->totalMoves; i++) {
        MoveLogEntry entry;
        recv(clientSocket, &entry, sizeof(entry), 0);

        std::cout << CYAN << "║ " << RESET;
        std::cout << std::setw(4) << entry.moveNumber << " │ ";
        std::cout << std::setw(6) << entry.playerId << " │ ";
        std::cout << "(" << std::setw(2) << (int)entry.x << "," << std::setw(2)
                  << (int)entry.y << ")    │ ";
        std::cout << std::setw(8) << entry.timestamp;
        std::cout << CYAN << " ║" << RESET << std::endl;
      }

      std::cout << CYAN
                << "╚═══════════════════════════════════════════════════════╝"
                << RESET << std::endl;
      break;
    }

    case MSG_ERROR: {
      std::cout << std::endl;
      std::cout << RED << "[Error] " << payload << RESET << std::endl;
      break;
    }

    default:
      std::cout << YELLOW << "[?] Unknown message type: " << header.type
                << RESET << std::endl;
    }
  }

  // ==================== MENU ====================

  void showMenu() {
    std::cout << std::endl;
    if (inGame) {
      std::cout << YELLOW << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║          IN-GAME MENU                 ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << RESET
                << std::endl;
      std::cout << YELLOW << "║" << RESET << "  1. Make Move"
                << (isMyTurn ? GREEN " (Your turn!)" : RED " (Wait...)")
                << RESET << std::setw(isMyTurn ? 8 : 12) << "" << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "║" << RESET
                << "  2. Offer Draw                       " << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "║" << RESET
                << "  3. Accept Draw                      " << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "║" << RESET
                << "  4. Decline Draw                     " << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "║" << RESET
                << "  5. Resign                           " << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "║" << RESET
                << "  9. Show Board                       " << YELLOW << "║"
                << RESET << std::endl;
      std::cout << YELLOW << "╚═══════════════════════════════════════╝"
                << RESET << std::endl;
    } else {
      std::cout << CYAN << "╔═══════════════════════════════════════╗"
                << std::endl;
      std::cout << "║            MAIN MENU                  ║" << std::endl;
      std::cout << "╠═══════════════════════════════════════╣" << RESET
                << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  1. Register                         " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  2. Login                            " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  3. View Online Players              " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  4. Send Challenge                   " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  5. Accept Challenge                 " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  6. Decline Challenge                " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "╠═══════════════════════════════════════╣" << RESET
                << std::endl;
      std::cout << CYAN << "║" << RESET
                << " 10. View Game History                " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << " 11. Request Rematch                  " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << " 12. Accept Rematch                   " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << " 13. Decline Rematch                  " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "║" << RESET
                << " 14. View Game Log                    " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "╠═══════════════════════════════════════╣" << RESET
                << std::endl;
      std::cout << CYAN << "║" << RESET
                << "  0. Exit                             " << CYAN << "║"
                << RESET << std::endl;
      std::cout << CYAN << "╚═══════════════════════════════════════╝" << RESET
                << std::endl;
      printUserInfo();
    }
    std::cout << std::endl << "Choice: ";
  }

  void run() {
    int choice;
    uint32_t tempId;

    clearScreen();
    printHeader();

    while (connected) {
      showMenu();
      choice = getIntInput();

      if (inGame) {
        // In-game menu
        switch (choice) {
        case 1:
          makeMove();
          break;
        case 2:
          offerDraw();
          break;
        case 3:
          acceptDraw();
          break;
        case 4:
          declineDraw();
          break;
        case 5:
          resign();
          break;
        case 9:
          displayBoard();
          break;
        default:
          std::cout << RED << "Invalid choice for in-game menu" << RESET
                    << std::endl;
        }
      } else {
        // Main menu
        switch (choice) {
        case 1:
          registerUser();
          break;
        case 2:
          login();
          break;
        case 3:
          getOnlinePlayers();
          break;
        case 4:
          sendChallenge();
          break;
        case 5:
          std::cout << "Challenge ID: ";
          tempId = getIntInput();
          acceptChallenge(tempId);
          break;
        case 6:
          std::cout << "Challenge ID: ";
          tempId = getIntInput();
          declineChallenge(tempId);
          break;
        case 10:
          getGameHistory();
          break;
        case 11:
          requestRematch();
          break;
        case 12:
          std::cout << "Game ID for rematch: ";
          tempId = getIntInput();
          acceptRematch(tempId);
          break;
        case 13:
          std::cout << "Game ID for rematch: ";
          tempId = getIntInput();
          declineRematch(tempId);
          break;
        case 14:
          getGameLog();
          break;
        case 0:
          connected = false;
          std::cout << YELLOW << "Goodbye!" << RESET << std::endl;
          break;
        default:
          std::cout << RED << "Invalid choice" << RESET << std::endl;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  ~GomokuClient() {
    if (gameBoard)
      delete[] gameBoard;
    close(clientSocket);
  }
};

int main(int argc, char *argv[]) {
  const char *host = "127.0.0.1";
  int port = 8888;

  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    port = std::atoi(argv[2]);
  }

  GomokuClient client;

  client.clearScreen();
  client.printHeader();

  std::cout << CYAN << "Connecting to " << host << ":" << port << "..." << RESET
            << std::endl;

  if (client.connectToServer(host, port)) {
    client.run();
  }

  return 0;
}
