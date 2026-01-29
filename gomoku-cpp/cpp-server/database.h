#ifndef DATABASE_H
#define DATABASE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

struct User {
  uint32_t userId;
  std::string username;
  std::string email;
  std::string passwordHash; // Store hashed password
  uint16_t eloRating;
  uint16_t wins;
  uint16_t losses;
  uint16_t draws;
  bool isOnline;
  bool inGame;
};

struct Challenge {
  uint32_t challengeId;
  uint32_t challengerId;
  uint32_t challengedId;
  uint8_t boardSize;
  uint16_t timeLimit;
  bool pending;
};

struct MoveLog {
  uint32_t moveNumber;
  uint32_t playerId;
  uint8_t x;
  uint8_t y;
  uint32_t timestamp; // Seconds since game start
};

struct GameRecord {
  uint32_t gameId;
  uint32_t player1Id;
  uint32_t player2Id;
  std::string player1Name;
  std::string player2Name;
  uint8_t boardSize;
  uint32_t winnerId;
  uint8_t result; // 0 = player1 win, 1 = player2 win, 2 = draw
  std::vector<MoveLog> moves;
  uint64_t startTime;
  uint32_t duration; // In seconds
  int16_t eloChange;
};

class Database {
private:
  std::map<uint32_t, User> users;
  std::map<std::string, uint32_t> usernameToId; // username -> userId mapping
  std::map<uint32_t, Challenge> challenges;
  std::map<uint32_t, GameRecord> gameRecords;
  uint32_t userIdCounter;
  uint32_t challengeIdCounter;
  uint32_t gameIdCounter;

  const std::string DATA_DIR = "./data/";
  const std::string USERS_FILE = "users.dat";
  const std::string GAMES_FILE = "games.dat";

public:
  Database() : userIdCounter(1), challengeIdCounter(1), gameIdCounter(1) {
    // Create data directory if not exists
    mkdir(DATA_DIR.c_str(), 0755);

    // Load existing data
    loadUsers();
    loadGames();

    std::cout << "Database initialized. Users: " << users.size()
              << ", Games: " << gameRecords.size() << std::endl;
  }

  // ==================== USER MANAGEMENT ====================

  bool createUser(const char *username, const char *email,
                  const char *password) {
    // Check if username exists
    if (usernameToId.find(username) != usernameToId.end()) {
      return false;
    }

    User user;
    user.userId = userIdCounter++;
    user.username = username;
    user.email = email;
    user.passwordHash = hashPassword(password); // Simple hash
    user.eloRating = 1000;
    user.wins = 0;
    user.losses = 0;
    user.draws = 0;
    user.isOnline = false;
    user.inGame = false;

    users[user.userId] = user;
    usernameToId[username] = user.userId;

    saveUsers();
    return true;
  }

  bool authenticateUser(const char *username, const char *password,
                        User &user) {
    auto it = usernameToId.find(username);
    if (it == usernameToId.end()) {
      return false;
    }

    User &storedUser = users[it->second];
    if (storedUser.passwordHash == hashPassword(password)) {
      user = storedUser;
      return true;
    }
    return false;
  }

  User getUser(uint32_t userId) {
    auto it = users.find(userId);
    if (it != users.end()) {
      return it->second;
    }
    return User();
  }

  std::vector<User> getOnlineUsers() {
    std::vector<User> onlineUsers;
    for (const auto &pair : users) {
      if (pair.second.isOnline) {
        onlineUsers.push_back(pair.second);
      }
    }
    return onlineUsers;
  }

  void setUserOnline(uint32_t userId, bool online) {
    auto it = users.find(userId);
    if (it != users.end()) {
      it->second.isOnline = online;
      if (!online) {
        it->second.inGame = false;
      }
    }
  }

  void setUserInGame(uint32_t userId, bool inGame) {
    auto it = users.find(userId);
    if (it != users.end()) {
      it->second.inGame = inGame;
    }
  }

  // ==================== CHALLENGE MANAGEMENT ====================

  uint32_t createChallenge(uint32_t challengerId, uint32_t challengedId,
                           uint8_t boardSize, uint16_t timeLimit) {
    Challenge challenge;
    challenge.challengeId = challengeIdCounter++;
    challenge.challengerId = challengerId;
    challenge.challengedId = challengedId;
    challenge.boardSize = boardSize;
    challenge.timeLimit = timeLimit;
    challenge.pending = true;

    challenges[challenge.challengeId] = challenge;
    return challenge.challengeId;
  }

  Challenge getChallenge(uint32_t challengeId) {
    auto it = challenges.find(challengeId);
    if (it != challenges.end()) {
      return it->second;
    }
    return Challenge();
  }

  void removeChallenge(uint32_t challengeId) { challenges.erase(challengeId); }

  // ==================== GAME MANAGEMENT ====================

  uint32_t createGame(uint32_t player1Id, uint32_t player2Id, uint8_t boardSize,
                      uint16_t timeLimit) {
    (void)timeLimit; // Stored in GameState, not in record
    GameRecord record;
    record.gameId = gameIdCounter++;
    record.player1Id = player1Id;
    record.player2Id = player2Id;
    record.player1Name = getUser(player1Id).username;
    record.player2Name = getUser(player2Id).username;
    record.boardSize = boardSize;
    record.winnerId = 0;
    record.result = 255; // Game in progress
    record.startTime = std::time(nullptr);
    record.duration = 0;
    record.eloChange = 0;

    gameRecords[record.gameId] = record;

    // Mark players as in game
    setUserInGame(player1Id, true);
    setUserInGame(player2Id, true);

    return record.gameId;
  }

  void logMove(uint32_t gameId, uint32_t playerId, uint32_t moveNumber,
               uint8_t x, uint8_t y) {
    auto it = gameRecords.find(gameId);
    if (it != gameRecords.end()) {
      MoveLog log;
      log.moveNumber = moveNumber;
      log.playerId = playerId;
      log.x = x;
      log.y = y;
      log.timestamp = std::time(nullptr) - it->second.startTime;

      it->second.moves.push_back(log);

      std::cout << "Move logged: Game " << gameId << ", Player " << playerId
                << ", Move " << moveNumber << " at (" << (int)x << "," << (int)y
                << ")" << std::endl;
    }
  }

  void updateGameResult(uint32_t gameId, uint32_t winnerId, uint8_t result) {
    auto it = gameRecords.find(gameId);
    if (it != gameRecords.end()) {
      it->second.winnerId = winnerId;
      it->second.result = result;
      it->second.duration = std::time(nullptr) - it->second.startTime;

      // Mark players as not in game
      setUserInGame(it->second.player1Id, false);
      setUserInGame(it->second.player2Id, false);

      saveGames();

      std::cout << "Game " << gameId << " completed. Winner: " << winnerId
                << ", Result: " << (int)result << std::endl;
    }
  }

  GameRecord getGameRecord(uint32_t gameId) {
    auto it = gameRecords.find(gameId);
    if (it != gameRecords.end()) {
      return it->second;
    }
    return GameRecord();
  }

  std::vector<GameRecord> getUserGameHistory(uint32_t userId,
                                             uint32_t limit = 20) {
    std::vector<GameRecord> history;

    for (const auto &pair : gameRecords) {
      if (pair.second.result != 255 && // Only completed games
          (pair.second.player1Id == userId ||
           pair.second.player2Id == userId)) {
        history.push_back(pair.second);
      }
    }

    // Sort by start time (newest first)
    std::sort(history.begin(), history.end(),
              [](const GameRecord &a, const GameRecord &b) {
                return a.startTime > b.startTime;
              });

    // Limit results
    if (history.size() > limit) {
      history.resize(limit);
    }

    return history;
  }

  // ==================== ELO RATING ====================

  int16_t updateEloRating(uint32_t winnerId, uint32_t loserId) {
    auto winnerIt = users.find(winnerId);
    auto loserIt = users.find(loserId);

    if (winnerIt == users.end() || loserIt == users.end()) {
      return 0;
    }

    int winnerElo = winnerIt->second.eloRating;
    int loserElo = loserIt->second.eloRating;

    // Calculate ELO change using standard formula
    const int K = 32;
    double expectedWinner =
        1.0 / (1.0 + pow(10.0, (loserElo - winnerElo) / 400.0));
    int16_t eloChange = (int16_t)(K * (1.0 - expectedWinner));

    // Update ratings
    winnerIt->second.eloRating += eloChange;
    loserIt->second.eloRating -= eloChange;

    // Update win/loss counts
    winnerIt->second.wins++;
    loserIt->second.losses++;

    saveUsers();

    return eloChange;
  }

  void updateDrawStats(uint32_t player1Id, uint32_t player2Id) {
    auto p1It = users.find(player1Id);
    auto p2It = users.find(player2Id);

    if (p1It != users.end()) {
      p1It->second.draws++;
    }
    if (p2It != users.end()) {
      p2It->second.draws++;
    }

    saveUsers();
  }

  // ==================== PERSISTENCE ====================

private:
  std::string hashPassword(const char *password) {
    // Simple hash for demo - in production use bcrypt or similar
    std::string pwd(password);
    uint32_t hash = 0;
    for (char c : pwd) {
      hash = hash * 31 + c;
    }
    return std::to_string(hash);
  }

  void saveUsers() {
    std::ofstream file(DATA_DIR + USERS_FILE);
    if (!file)
      return;

    file << userIdCounter << "\n";
    file << users.size() << "\n";

    for (const auto &pair : users) {
      const User &u = pair.second;
      file << u.userId << "|" << u.username << "|" << u.email << "|"
           << u.passwordHash << "|" << u.eloRating << "|" << u.wins << "|"
           << u.losses << "|" << u.draws << "\n";
    }

    file.close();
  }

  void loadUsers() {
    std::ifstream file(DATA_DIR + USERS_FILE);
    if (!file)
      return;

    std::string line;

    // Read counter
    std::getline(file, line);
    userIdCounter = std::stoul(line);

    // Read user count
    std::getline(file, line);
    size_t count = std::stoul(line);

    for (size_t i = 0; i < count; i++) {
      std::getline(file, line);
      std::istringstream iss(line);
      std::string token;
      User u;

      std::getline(iss, token, '|');
      u.userId = std::stoul(token);
      std::getline(iss, u.username, '|');
      std::getline(iss, u.email, '|');
      std::getline(iss, u.passwordHash, '|');
      std::getline(iss, token, '|');
      u.eloRating = std::stoi(token);
      std::getline(iss, token, '|');
      u.wins = std::stoi(token);
      std::getline(iss, token, '|');
      u.losses = std::stoi(token);
      std::getline(iss, token, '|');
      u.draws = std::stoi(token);

      u.isOnline = false;
      u.inGame = false;

      users[u.userId] = u;
      usernameToId[u.username] = u.userId;
    }

    file.close();
  }

  void saveGames() {
    std::ofstream file(DATA_DIR + GAMES_FILE);
    if (!file)
      return;

    file << gameIdCounter << "\n";

    // Only save completed games
    uint32_t completedCount = 0;
    for (const auto &pair : gameRecords) {
      if (pair.second.result != 255)
        completedCount++;
    }
    file << completedCount << "\n";

    for (const auto &pair : gameRecords) {
      const GameRecord &g = pair.second;
      if (g.result == 255)
        continue; // Skip ongoing games

      file << g.gameId << "|" << g.player1Id << "|" << g.player2Id << "|"
           << g.player1Name << "|" << g.player2Name << "|" << (int)g.boardSize
           << "|" << g.winnerId << "|" << (int)g.result << "|" << g.startTime
           << "|" << g.duration << "|" << g.eloChange << "|" << g.moves.size()
           << "\n";

      // Save moves
      for (const auto &m : g.moves) {
        file << m.moveNumber << "," << m.playerId << "," << (int)m.x << ","
             << (int)m.y << "," << m.timestamp << ";";
      }
      file << "\n";
    }

    file.close();
  }

  void loadGames() {
    std::ifstream file(DATA_DIR + GAMES_FILE);
    if (!file)
      return;

    std::string line;

    // Read counter
    std::getline(file, line);
    if (line.empty())
      return;
    gameIdCounter = std::stoul(line);

    // Read game count
    std::getline(file, line);
    if (line.empty())
      return;
    size_t count = std::stoul(line);

    for (size_t i = 0; i < count; i++) {
      std::getline(file, line);
      if (line.empty())
        continue;

      std::istringstream iss(line);
      std::string token;
      GameRecord g;

      std::getline(iss, token, '|');
      g.gameId = std::stoul(token);
      std::getline(iss, token, '|');
      g.player1Id = std::stoul(token);
      std::getline(iss, token, '|');
      g.player2Id = std::stoul(token);
      std::getline(iss, g.player1Name, '|');
      std::getline(iss, g.player2Name, '|');
      std::getline(iss, token, '|');
      g.boardSize = std::stoi(token);
      std::getline(iss, token, '|');
      g.winnerId = std::stoul(token);
      std::getline(iss, token, '|');
      g.result = std::stoi(token);
      std::getline(iss, token, '|');
      g.startTime = std::stoull(token);
      std::getline(iss, token, '|');
      g.duration = std::stoul(token);
      std::getline(iss, token, '|');
      g.eloChange = std::stoi(token);
      std::getline(iss, token, '|');
      size_t moveCount = std::stoul(token);

      // Load moves
      std::getline(file, line);
      if (!line.empty() && moveCount > 0) {
        std::istringstream movesStream(line);
        std::string moveStr;
        while (std::getline(movesStream, moveStr, ';')) {
          if (moveStr.empty())
            continue;
          std::istringstream moveIss(moveStr);
          std::string mToken;
          MoveLog m;

          std::getline(moveIss, mToken, ',');
          m.moveNumber = std::stoul(mToken);
          std::getline(moveIss, mToken, ',');
          m.playerId = std::stoul(mToken);
          std::getline(moveIss, mToken, ',');
          m.x = std::stoi(mToken);
          std::getline(moveIss, mToken, ',');
          m.y = std::stoi(mToken);
          std::getline(moveIss, mToken, ',');
          m.timestamp = std::stoul(mToken);

          g.moves.push_back(m);
        }
      }

      gameRecords[g.gameId] = g;
    }

    file.close();
  }

public:
  ~Database() {
    saveUsers();
    saveGames();
    std::cout << "Database saved and closed" << std::endl;
  }
};

#endif
