#include "database.h"
#include "game_logic.h"
#include "protocol.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

class GomokuServer {
private:
  int serverSocket;
  std::map<int, uint32_t> clientSockets; // socket -> userId
  std::map<uint32_t, int> userSockets;   // userId -> socket
  std::map<uint32_t, GameState *> activeGames;
  std::map<uint32_t, uint32_t> userToGame; // userId -> gameId
  std::map<uint32_t, RematchRequest>
      pendingRematches; // gameId -> rematch request
  std::mutex clientMutex;
  std::mutex gameMutex;
  Database db;
  bool running;

public:
  GomokuServer(int port) : running(true) {
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
      std::cerr << "Error creating socket" << std::endl;
      exit(1);
    }

    // Set socket options
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind socket
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
        0) {
      std::cerr << "Error binding socket" << std::endl;
      exit(1);
    }

    // Listen
    if (listen(serverSocket, 10) < 0) {
      std::cerr << "Error listening" << std::endl;
      exit(1);
    }

    std::cout << "╔══════════════════════════════════════════╗" << std::endl;
    std::cout << "║     GOMOKU SERVER - LAN MULTIPLAYER      ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Server started on port " << port << "            ║"
              << std::endl;
    std::cout << "║  Waiting for connections...              ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════╝" << std::endl;

    // Start timeout checker thread
    std::thread(&GomokuServer::timeoutChecker, this).detach();
  }

  void start() {
    while (running) {
      struct sockaddr_in clientAddr;
      socklen_t clientLen = sizeof(clientAddr);

      int clientSocket =
          accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
      if (clientSocket < 0) {
        if (running) {
          std::cerr << "Error accepting connection" << std::endl;
        }
        continue;
      }

      std::cout << "[+] New client connected: " << clientSocket << std::endl;

      // Handle client in new thread
      std::thread(&GomokuServer::handleClient, this, clientSocket).detach();
    }
  }

  void timeoutChecker() {
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      std::lock_guard<std::mutex> lock(gameMutex);
      for (auto &pair : activeGames) {
        GameState *game = pair.second;
        if (game->timeLimit > 0 && GameLogic::checkTimeout(game)) {
          // Current player timed out
          uint32_t loserId = game->currentTurn;
          uint32_t winnerId =
              (game->player1Id == loserId) ? game->player2Id : game->player1Id;

          handleGameOver(game, winnerId, 2); // 2 = timeout
        }
      }
    }
  }

  void handleClient(int clientSocket) {
    while (running) {
      MessageHeader header;
      int bytesRead = recv(clientSocket, &header, sizeof(header), 0);

      if (bytesRead <= 0) {
        std::cout << "[-] Client disconnected: " << clientSocket << std::endl;
        removeClient(clientSocket);
        close(clientSocket);
        break;
      }

      // Read payload
      char *payload = nullptr;
      if (header.length > 0) {
        payload = new char[header.length];
        bytesRead = recv(clientSocket, payload, header.length, 0);

        if (bytesRead <= 0) {
          delete[] payload;
          break;
        }
      }

      // Process message
      processMessage(clientSocket, header, payload);
      if (payload)
        delete[] payload;
    }
  }

  void processMessage(int clientSocket, MessageHeader &header, char *payload) {
    switch (header.type) {
    case MSG_REGISTER:
      handleRegister(clientSocket, (RegisterRequest *)payload);
      break;

    case MSG_LOGIN:
      handleLogin(clientSocket, (LoginRequest *)payload);
      break;

    case MSG_GET_ONLINE_PLAYERS:
      handleGetOnlinePlayers(clientSocket, header.userId);
      break;

    case MSG_SEND_CHALLENGE:
      handleSendChallenge(clientSocket, header.userId,
                          (ChallengeRequest *)payload);
      break;

    case MSG_ACCEPT_CHALLENGE:
      handleAcceptChallenge(clientSocket, header.userId, *(uint32_t *)payload);
      break;

    case MSG_DECLINE_CHALLENGE:
      handleDeclineChallenge(clientSocket, header.userId, *(uint32_t *)payload);
      break;

    case MSG_MAKE_MOVE:
      handleMakeMove(clientSocket, header.userId, (MoveRequest *)payload);
      break;

    case MSG_RESIGN:
      handleResign(clientSocket, header.userId, (ResignRequest *)payload);
      break;

    case MSG_OFFER_DRAW:
      handleOfferDraw(clientSocket, header.userId, (DrawRequest *)payload);
      break;

    case MSG_ACCEPT_DRAW:
      handleAcceptDraw(clientSocket, header.userId, (DrawRequest *)payload);
      break;

    case MSG_DECLINE_DRAW:
      handleDeclineDraw(clientSocket, header.userId, (DrawRequest *)payload);
      break;

    case MSG_REQUEST_REMATCH:
      handleRequestRematch(clientSocket, header.userId,
                           (RematchRequest *)payload);
      break;

    case MSG_ACCEPT_REMATCH:
      handleAcceptRematch(clientSocket, header.userId, *(uint32_t *)payload);
      break;

    case MSG_DECLINE_REMATCH:
      handleDeclineRematch(clientSocket, header.userId, *(uint32_t *)payload);
      break;

    case MSG_GET_GAME_LOG:
      handleGetGameLog(clientSocket, header.userId, *(uint32_t *)payload);
      break;

    case MSG_GET_GAME_HISTORY:
      handleGetGameHistory(clientSocket, header.userId);
      break;

    default:
      std::cerr << "Unknown message type: " << header.type << std::endl;
    }
  }

  // ==================== AUTHENTICATION ====================

  void handleRegister(int clientSocket, RegisterRequest *req) {
    LoginResponse response;
    memset(&response, 0, sizeof(response));

    if (db.createUser(req->username, req->email, req->password)) {
      response.success = 1;
      strcpy(response.message, "Registration successful! Please login.");
    } else {
      response.success = 0;
      strcpy(response.message, "Username already exists");
    }

    sendMessage(clientSocket, MSG_REGISTER_RESPONSE, 0, 0, &response,
                sizeof(response));
  }

  void handleLogin(int clientSocket, LoginRequest *req) {
    LoginResponse response;
    memset(&response, 0, sizeof(response));

    User user;
    if (db.authenticateUser(req->username, req->password, user)) {
      response.success = 1;
      response.userId = user.userId;
      response.sessionId = generateSessionId();
      response.eloRating = user.eloRating;
      response.wins = user.wins;
      response.losses = user.losses;
      response.draws = user.draws;
      strcpy(response.message, "Login successful!");

      // Store client mapping
      std::lock_guard<std::mutex> lock(clientMutex);
      clientSockets[clientSocket] = user.userId;
      userSockets[user.userId] = clientSocket;

      db.setUserOnline(user.userId, true);

      std::cout << "[*] User logged in: " << user.username
                << " (ID: " << user.userId << ")" << std::endl;
    } else {
      response.success = 0;
      strcpy(response.message, "Invalid username or password");
    }

    sendMessage(clientSocket, MSG_LOGIN_RESPONSE, 0, 0, &response,
                sizeof(response));
  }

  // ==================== PLAYER LIST ====================

  void handleGetOnlinePlayers(int clientSocket, uint32_t userId) {
    std::vector<User> onlineUsers = db.getOnlineUsers();

    // Count players excluding self
    uint32_t count = 0;
    for (const auto &user : onlineUsers) {
      if (user.userId != userId)
        count++;
    }

    sendMessage(clientSocket, MSG_ONLINE_PLAYERS_LIST, userId, 0, &count,
                sizeof(count));

    // Send each player info
    for (const auto &user : onlineUsers) {
      if (user.userId != userId) {
        PlayerInfo info;
        info.userId = user.userId;
        strcpy(info.username, user.username.c_str());
        info.eloRating = user.eloRating;
        info.wins = user.wins;
        info.losses = user.losses;
        info.draws = user.draws;
        info.isOnline = 1;
        info.inGame = user.inGame ? 1 : 0;

        send(clientSocket, &info, sizeof(info), 0);
      }
    }
  }

  // ==================== CHALLENGE SYSTEM ====================

  void handleSendChallenge(int clientSocket, uint32_t challengerId,
                           ChallengeRequest *req) {
    uint32_t challengeId = db.createChallenge(challengerId, req->targetUserId,
                                              req->boardSize, req->timeLimit);

    // Send to target user
    std::lock_guard<std::mutex> lock(clientMutex);
    auto it = userSockets.find(req->targetUserId);
    if (it != userSockets.end()) {
      ChallengeResponse response;
      response.challengeId = challengeId;
      response.challengerId = challengerId;

      User challenger = db.getUser(challengerId);
      strcpy(response.challengerName, challenger.username.c_str());
      response.boardSize = req->boardSize;
      response.timeLimit = req->timeLimit;

      sendMessage(it->second, MSG_CHALLENGE_RECEIVED, 0, 0, &response,
                  sizeof(response));

      std::cout << "[*] Challenge sent: " << challenger.username << " -> User "
                << req->targetUserId << std::endl;
    }

    // Confirm to sender
    sendMessage(clientSocket, MSG_CHALLENGE_RESPONSE, challengerId, 0,
                &challengeId, sizeof(challengeId));
  }

  void handleAcceptChallenge(int clientSocket, uint32_t userId,
                             uint32_t challengeId) {
    Challenge challenge = db.getChallenge(challengeId);
    if (challenge.challengeId == 0) {
      sendError(clientSocket, "Challenge not found or expired");
      return;
    }

    db.removeChallenge(challengeId);

    // Create game
    uint32_t gameId = db.createGame(challenge.challengerId, userId,
                                    challenge.boardSize, challenge.timeLimit);

    std::lock_guard<std::mutex> lock(gameMutex);

    GameState *game = new GameState();
    game->gameId = gameId;
    game->player1Id = challenge.challengerId;
    game->player2Id = userId;
    game->boardSize = challenge.boardSize;
    game->board = new uint8_t[challenge.boardSize * challenge.boardSize]();
    game->currentTurn = challenge.challengerId;
    game->timeLimit = challenge.timeLimit;
    game->player1TimeLeft = challenge.timeLimit;
    game->player2TimeLeft = challenge.timeLimit;
    game->lastMoveTime = std::chrono::steady_clock::now();
    game->timerActive = (challenge.timeLimit > 0);

    activeGames[gameId] = game;
    userToGame[challenge.challengerId] = gameId;
    userToGame[userId] = gameId;

    // Get player names
    User player1 = db.getUser(challenge.challengerId);
    User player2 = db.getUser(userId);

    // Send game start to both players
    GameStart startMsg;
    startMsg.gameId = gameId;
    startMsg.player1Id = challenge.challengerId;
    startMsg.player2Id = userId;
    strcpy(startMsg.player1Name, player1.username.c_str());
    strcpy(startMsg.player2Name, player2.username.c_str());
    startMsg.boardSize = challenge.boardSize;
    startMsg.currentTurn = challenge.challengerId;
    startMsg.timeLimit = challenge.timeLimit;
    startMsg.player1Time = challenge.timeLimit;
    startMsg.player2Time = challenge.timeLimit;

    std::lock_guard<std::mutex> clientLock(clientMutex);
    auto p1Socket = userSockets.find(challenge.challengerId);
    auto p2Socket = userSockets.find(userId);

    if (p1Socket != userSockets.end()) {
      sendMessage(p1Socket->second, MSG_GAME_START, challenge.challengerId, 0,
                  &startMsg, sizeof(startMsg));
    }
    if (p2Socket != userSockets.end()) {
      sendMessage(p2Socket->second, MSG_GAME_START, userId, 0, &startMsg,
                  sizeof(startMsg));
    }

    std::cout << "[*] Game started: " << player1.username << " vs "
              << player2.username << " (Game #" << gameId << ")" << std::endl;
  }

  void handleDeclineChallenge(int clientSocket, uint32_t userId,
                              uint32_t challengeId) {
    (void)clientSocket; // Not used in this handler
    Challenge challenge = db.getChallenge(challengeId);
    if (challenge.challengeId == 0) {
      return;
    }

    db.removeChallenge(challengeId);

    // Notify challenger
    std::lock_guard<std::mutex> lock(clientMutex);
    auto it = userSockets.find(challenge.challengerId);
    if (it != userSockets.end()) {
      ChallengeDeclinedResponse response;
      response.challengeId = challengeId;
      response.declinerId = userId;

      User decliner = db.getUser(userId);
      strcpy(response.declinerName, decliner.username.c_str());

      sendMessage(it->second, MSG_CHALLENGE_DECLINED, 0, 0, &response,
                  sizeof(response));

      std::cout << "[*] Challenge declined by " << decliner.username
                << std::endl;
    }
  }

  // ==================== GAME PLAY ====================

  void handleMakeMove(int clientSocket, uint32_t userId, MoveRequest *req) {
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = activeGames.find(req->gameId);
    if (it == activeGames.end()) {
      sendError(clientSocket, "Game not found");
      return;
    }

    GameState *game = it->second;

    // Validate turn
    if (game->currentTurn != userId) {
      sendError(clientSocket, "Not your turn");
      return;
    }

    // Check timeout before move
    if (game->timeLimit > 0 && GameLogic::checkTimeout(game)) {
      uint32_t winnerId =
          (game->player1Id == userId) ? game->player2Id : game->player1Id;
      handleGameOver(game, winnerId, 2);
      return;
    }

    // Validate move
    if (!GameLogic::isValidMove(game, req->x, req->y)) {
      sendError(clientSocket, "Invalid move - cell occupied or out of bounds");
      return;
    }

    // Update time
    GameLogic::updateTimeAfterMove(game);

    // Make move
    uint8_t player = (game->player1Id == userId) ? 1 : 2;
    game->board[req->y * game->boardSize + req->x] = player;
    game->moveCount++;

    // Log move
    db.logMove(req->gameId, userId, game->moveCount, req->x, req->y);

    // Check win
    if (GameLogic::checkWin(game, req->x, req->y, player)) {
      handleGameOver(game, userId, 0); // 0 = normal win
      return;
    }

    // Check draw (board full)
    if (GameLogic::checkDraw(game)) {
      handleGameDraw(game);
      return;
    }

    // Continue game
    game->currentTurn =
        (game->player1Id == userId) ? game->player2Id : game->player1Id;
    game->lastMoveTime = std::chrono::steady_clock::now();

    // Clear draw offer when a move is made
    game->drawOffered = false;
    game->drawOfferedBy = 0;

    MoveResponse response;
    response.success = 1;
    response.x = req->x;
    response.y = req->y;
    response.player = player;
    response.nextTurn = game->currentTurn;
    response.player1Time = GameLogic::getRemainingTime(game, game->player1Id);
    response.player2Time = GameLogic::getRemainingTime(game, game->player2Id);
    response.moveNumber = game->moveCount;

    // Send to both players
    std::lock_guard<std::mutex> clientLock(clientMutex);
    auto p1Socket = userSockets.find(game->player1Id);
    auto p2Socket = userSockets.find(game->player2Id);

    if (p1Socket != userSockets.end()) {
      sendMessage(p1Socket->second, MSG_MOVE_RESPONSE, game->player1Id, 0,
                  &response, sizeof(response));
    }
    if (p2Socket != userSockets.end()) {
      sendMessage(p2Socket->second, MSG_OPPONENT_MOVE, game->player2Id, 0,
                  &response, sizeof(response));
    }
  }

  // ==================== RESIGN / DRAW ====================

  void handleResign(int clientSocket, uint32_t userId, ResignRequest *req) {
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = activeGames.find(req->gameId);
    if (it == activeGames.end()) {
      sendError(clientSocket, "Game not found");
      return;
    }

    GameState *game = it->second;
    uint32_t winnerId =
        (game->player1Id == userId) ? game->player2Id : game->player1Id;

    User resigner = db.getUser(userId);
    std::cout << "[*] " << resigner.username << " resigned from Game #"
              << req->gameId << std::endl;

    handleGameOver(game, winnerId, 1); // 1 = resign
  }

  void handleOfferDraw(int clientSocket, uint32_t userId, DrawRequest *req) {
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = activeGames.find(req->gameId);
    if (it == activeGames.end()) {
      sendError(clientSocket, "Game not found");
      return;
    }

    GameState *game = it->second;

    if (game->drawOffered) {
      sendError(clientSocket, "Draw already offered");
      return;
    }

    game->drawOffered = true;
    game->drawOfferedBy = userId;

    // Notify opponent
    uint32_t opponentId =
        (game->player1Id == userId) ? game->player2Id : game->player1Id;

    std::lock_guard<std::mutex> clientLock(clientMutex);
    auto opponentSocket = userSockets.find(opponentId);
    if (opponentSocket != userSockets.end()) {
      sendMessage(opponentSocket->second, MSG_DRAW_RECEIVED, 0, 0, req,
                  sizeof(DrawRequest));
    }

    User offerer = db.getUser(userId);
    std::cout << "[*] " << offerer.username << " offered a draw in Game #"
              << req->gameId << std::endl;
  }

  void handleAcceptDraw(int clientSocket, uint32_t userId, DrawRequest *req) {
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = activeGames.find(req->gameId);
    if (it == activeGames.end()) {
      sendError(clientSocket, "Game not found");
      return;
    }

    GameState *game = it->second;

    if (!game->drawOffered || game->drawOfferedBy == userId) {
      sendError(clientSocket, "No draw offer to accept");
      return;
    }

    handleGameDraw(game);
  }

  void handleDeclineDraw(int clientSocket, uint32_t userId, DrawRequest *req) {
    (void)clientSocket; // Not used in this handler
    std::lock_guard<std::mutex> lock(gameMutex);

    auto it = activeGames.find(req->gameId);
    if (it == activeGames.end()) {
      return;
    }

    GameState *game = it->second;
    game->drawOffered = false;
    game->drawOfferedBy = 0;

    // Notify offerer
    uint32_t offererId =
        (game->player1Id == userId) ? game->player2Id : game->player1Id;

    std::lock_guard<std::mutex> clientLock(clientMutex);
    auto offererSocket = userSockets.find(offererId);
    if (offererSocket != userSockets.end()) {
      DrawRequest response;
      response.gameId = req->gameId;
      sendMessage(offererSocket->second, MSG_DECLINE_DRAW, 0, 0, &response,
                  sizeof(response));
    }
  }

  // ==================== REMATCH ====================

  void handleRequestRematch(int clientSocket, uint32_t userId,
                            RematchRequest *req) {
    (void)clientSocket; // Not used in this handler
    (void)userId;       // Logged from req->opponentId instead
    // Store rematch request
    pendingRematches[req->lastGameId] = *req;

    // Notify opponent
    std::lock_guard<std::mutex> lock(clientMutex);
    auto opponentSocket = userSockets.find(req->opponentId);
    if (opponentSocket != userSockets.end()) {
      sendMessage(opponentSocket->second, MSG_REMATCH_RECEIVED, 0, 0, req,
                  sizeof(RematchRequest));
    }

    User requester = db.getUser(userId);
    std::cout << "[*] " << requester.username << " requested rematch"
              << std::endl;
  }

  void handleAcceptRematch(int clientSocket, uint32_t userId,
                           uint32_t lastGameId) {
    auto it = pendingRematches.find(lastGameId);
    if (it == pendingRematches.end()) {
      sendError(clientSocket, "Rematch request not found");
      return;
    }

    RematchRequest req = it->second;
    pendingRematches.erase(it);

    // Get previous game settings
    GameRecord prevGame = db.getGameRecord(lastGameId);

    // Create new challenge and accept it automatically
    uint32_t challengeId = db.createChallenge(
        req.opponentId, userId, prevGame.boardSize, 0); // Reset time
    handleAcceptChallenge(clientSocket, userId, challengeId);
  }

  void handleDeclineRematch(int clientSocket, uint32_t userId,
                            uint32_t lastGameId) {
    (void)clientSocket; // Not used in this handler
    (void)userId;       // Not used in this handler
    auto it = pendingRematches.find(lastGameId);
    if (it == pendingRematches.end()) {
      return;
    }

    RematchRequest req = it->second;
    pendingRematches.erase(it);

    // Notify requester
    std::lock_guard<std::mutex> lock(clientMutex);
    auto requesterSocket = userSockets.find(req.opponentId);
    if (requesterSocket != userSockets.end()) {
      sendMessage(requesterSocket->second, MSG_REMATCH_DECLINED, 0, 0,
                  &lastGameId, sizeof(lastGameId));
    }
  }

  // ==================== GAME LOGS & HISTORY ====================

  void handleGetGameLog(int clientSocket, uint32_t userId, uint32_t gameId) {
    GameRecord record = db.getGameRecord(gameId);
    if (record.gameId == 0) {
      sendError(clientSocket, "Game not found");
      return;
    }

    // Send header
    GameLogHeader header;
    header.gameId = record.gameId;
    header.player1Id = record.player1Id;
    header.player2Id = record.player2Id;
    strcpy(header.player1Name, record.player1Name.c_str());
    strcpy(header.player2Name, record.player2Name.c_str());
    header.boardSize = record.boardSize;
    header.winnerId = record.winnerId;
    header.result = record.result;
    header.totalMoves = record.moves.size();
    header.gameDuration = record.duration;
    header.timestamp = record.startTime;

    sendMessage(clientSocket, MSG_GAME_LOG_RESPONSE, userId, 0, &header,
                sizeof(header));

    // Send moves
    for (const auto &move : record.moves) {
      MoveLogEntry entry;
      entry.moveNumber = move.moveNumber;
      entry.playerId = move.playerId;
      entry.x = move.x;
      entry.y = move.y;
      entry.timestamp = move.timestamp;

      send(clientSocket, &entry, sizeof(entry), 0);
    }
  }

  void handleGetGameHistory(int clientSocket, uint32_t userId) {
    std::vector<GameRecord> history = db.getUserGameHistory(userId);

    uint32_t count = history.size();
    sendMessage(clientSocket, MSG_GAME_HISTORY_RESPONSE, userId, 0, &count,
                sizeof(count));

    for (const auto &record : history) {
      GameHistoryEntry entry;
      entry.gameId = record.gameId;
      entry.opponentId =
          (record.player1Id == userId) ? record.player2Id : record.player1Id;
      strcpy(entry.opponentName, (record.player1Id == userId)
                                     ? record.player2Name.c_str()
                                     : record.player1Name.c_str());

      if (record.result == 2) {
        entry.result = 2; // Draw
      } else if (record.winnerId == userId) {
        entry.result = 0; // Win
      } else {
        entry.result = 1; // Loss
      }

      entry.eloChange =
          (record.winnerId == userId) ? record.eloChange : -record.eloChange;
      entry.timestamp = record.startTime;

      send(clientSocket, &entry, sizeof(entry), 0);
    }
  }

  // ==================== GAME END HANDLING ====================

  void handleGameOver(GameState *game, uint32_t winnerId, uint8_t reason) {
    // Update database
    uint8_t result = (winnerId == game->player1Id) ? 0 : 1;
    db.updateGameResult(game->gameId, winnerId, result);

    // Update ELO
    uint32_t loserId =
        (game->player1Id == winnerId) ? game->player2Id : game->player1Id;
    int16_t eloChange = db.updateEloRating(winnerId, loserId);

    GameOver gameOver;
    gameOver.gameId = game->gameId;
    gameOver.winnerId = winnerId;

    User winner = db.getUser(winnerId);
    strcpy(gameOver.winnerName, winner.username.c_str());
    gameOver.eloChange = eloChange;
    gameOver.reason = reason;
    gameOver.totalMoves = game->moveCount;

    // Send to both players
    std::lock_guard<std::mutex> lock(clientMutex);
    auto p1Socket = userSockets.find(game->player1Id);
    auto p2Socket = userSockets.find(game->player2Id);

    if (p1Socket != userSockets.end()) {
      sendMessage(p1Socket->second, MSG_GAME_OVER, game->player1Id, 0,
                  &gameOver, sizeof(gameOver));
    }
    if (p2Socket != userSockets.end()) {
      sendMessage(p2Socket->second, MSG_GAME_OVER, game->player2Id, 0,
                  &gameOver, sizeof(gameOver));
    }

    std::cout << "[*] Game #" << game->gameId
              << " ended. Winner: " << winner.username
              << " (Reason: " << (int)reason << ")" << std::endl;

    // Cleanup
    cleanupGame(game->gameId);
  }

  void handleGameDraw(GameState *game) {
    // Update database
    db.updateGameResult(game->gameId, 0, 2); // 2 = draw
    db.updateDrawStats(game->player1Id, game->player2Id);

    GameOver gameOver;
    gameOver.gameId = game->gameId;
    gameOver.winnerId = 0;
    strcpy(gameOver.winnerName, "DRAW");
    gameOver.eloChange = 0;
    gameOver.reason = 3; // Draw
    gameOver.totalMoves = game->moveCount;

    // Send to both players
    std::lock_guard<std::mutex> lock(clientMutex);
    auto p1Socket = userSockets.find(game->player1Id);
    auto p2Socket = userSockets.find(game->player2Id);

    if (p1Socket != userSockets.end()) {
      sendMessage(p1Socket->second, MSG_GAME_OVER, game->player1Id, 0,
                  &gameOver, sizeof(gameOver));
    }
    if (p2Socket != userSockets.end()) {
      sendMessage(p2Socket->second, MSG_GAME_OVER, game->player2Id, 0,
                  &gameOver, sizeof(gameOver));
    }

    std::cout << "[*] Game #" << game->gameId << " ended in a DRAW"
              << std::endl;

    // Cleanup
    cleanupGame(game->gameId);
  }

  void cleanupGame(uint32_t gameId) {
    auto it = activeGames.find(gameId);
    if (it != activeGames.end()) {
      GameState *game = it->second;
      userToGame.erase(game->player1Id);
      userToGame.erase(game->player2Id);
      delete game;
      activeGames.erase(gameId);
    }
  }

  // ==================== UTILITIES ====================

  void sendMessage(int socket, uint16_t type, uint32_t userId,
                   uint32_t sessionId, void *payload, uint32_t length) {
    MessageHeader header;
    header.type = type;
    header.length = length;
    header.userId = userId;
    header.sessionId = sessionId;

    send(socket, &header, sizeof(header), 0);
    if (length > 0 && payload) {
      send(socket, payload, length, 0);
    }
  }

  void sendError(int socket, const char *message) {
    char errorMsg[128];
    strncpy(errorMsg, message, 127);
    errorMsg[127] = '\0';
    sendMessage(socket, MSG_ERROR, 0, 0, errorMsg, strlen(errorMsg) + 1);
  }

  void removeClient(int clientSocket) {
    std::lock_guard<std::mutex> lock(clientMutex);
    auto it = clientSockets.find(clientSocket);
    if (it != clientSockets.end()) {
      uint32_t userId = it->second;

      // Check if user is in a game
      auto gameIt = userToGame.find(userId);
      if (gameIt != userToGame.end()) {
        std::lock_guard<std::mutex> gameLock(gameMutex);
        auto activeGameIt = activeGames.find(gameIt->second);
        if (activeGameIt != activeGames.end()) {
          GameState *game = activeGameIt->second;
          uint32_t winnerId =
              (game->player1Id == userId) ? game->player2Id : game->player1Id;
          handleGameOver(game, winnerId, 1); // Treat as resign
        }
      }

      db.setUserOnline(userId, false);
      userSockets.erase(userId);
      clientSockets.erase(it);

      User user = db.getUser(userId);
      std::cout << "[*] User logged out: " << user.username << std::endl;
    }
  }

  uint32_t generateSessionId() {
    static uint32_t sessionCounter = 1000;
    return sessionCounter++;
  }

  ~GomokuServer() {
    running = false;
    close(serverSocket);
  }
};

int main(int argc, char *argv[]) {
  int port = 8888;
  if (argc > 1) {
    port = std::atoi(argv[1]);
  }

  GomokuServer server(port);
  server.start();
  return 0;
}
