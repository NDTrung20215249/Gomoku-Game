#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>

// Message Types
enum MessageType {
    // Authentication (2 points)
    MSG_REGISTER = 1,
    MSG_REGISTER_RESPONSE = 2,
    MSG_LOGIN = 3,
    MSG_LOGIN_RESPONSE = 4,
    MSG_LOGOUT = 5,
    
    // Player List (2 points)
    MSG_GET_ONLINE_PLAYERS = 10,
    MSG_ONLINE_PLAYERS_LIST = 11,
    
    // Challenge System (3 points - now complete with decline)
    MSG_SEND_CHALLENGE = 20,
    MSG_CHALLENGE_RECEIVED = 21,
    MSG_ACCEPT_CHALLENGE = 22,
    MSG_DECLINE_CHALLENGE = 23,
    MSG_CHALLENGE_RESPONSE = 24,
    MSG_CHALLENGE_DECLINED = 25,  // Notify challenger that challenge was declined
    
    // Game Play (10 points)
    MSG_GAME_START = 30,
    MSG_MAKE_MOVE = 31,
    MSG_MOVE_RESPONSE = 32,
    MSG_OPPONENT_MOVE = 33,
    MSG_GAME_OVER = 34,
    
    // Resignation/Draw (1 point)
    MSG_RESIGN = 40,
    MSG_OFFER_DRAW = 41,
    MSG_DRAW_RECEIVED = 42,
    MSG_ACCEPT_DRAW = 43,
    MSG_DECLINE_DRAW = 44,
    MSG_DRAW_RESULT = 45,
    
    // Rematch (1 point)
    MSG_REQUEST_REMATCH = 50,
    MSG_REMATCH_RECEIVED = 51,
    MSG_ACCEPT_REMATCH = 52,
    MSG_DECLINE_REMATCH = 53,
    MSG_REMATCH_DECLINED = 54,
    
    // Game Logs & Replay (2 points)
    MSG_GET_GAME_LOG = 60,
    MSG_GAME_LOG_RESPONSE = 61,
    MSG_GET_GAME_HISTORY = 62,
    MSG_GAME_HISTORY_RESPONSE = 63,
    MSG_REPLAY_GAME = 64,
    MSG_REPLAY_DATA = 65,
    
    // Time management
    MSG_TIME_UPDATE = 70,
    MSG_TIME_OUT = 71,
    
    // Error
    MSG_ERROR = 99
};

// Protocol Header (fixed size)
struct MessageHeader {
    uint16_t type;          // Message type
    uint32_t length;        // Payload length
    uint32_t userId;        // Sender user ID
    uint32_t sessionId;     // Session ID
} __attribute__((packed));

// Login Request
struct LoginRequest {
    char username[32];
    char password[64];
} __attribute__((packed));

// Login Response
struct LoginResponse {
    uint8_t success;
    uint32_t userId;
    uint32_t sessionId;
    uint16_t eloRating;
    uint16_t wins;
    uint16_t losses;
    uint16_t draws;
    char message[128];
} __attribute__((packed));

// Register Request
struct RegisterRequest {
    char username[32];
    char email[64];
    char password[64];
} __attribute__((packed));

// Challenge Request
struct ChallengeRequest {
    uint32_t targetUserId;
    uint8_t boardSize;
    uint16_t timeLimit;  // Time limit per player in seconds (0 = unlimited)
} __attribute__((packed));

// Challenge Response
struct ChallengeResponse {
    uint32_t challengeId;
    uint32_t challengerId;
    char challengerName[32];
    uint8_t boardSize;
    uint16_t timeLimit;
} __attribute__((packed));

// Challenge Declined Response
struct ChallengeDeclinedResponse {
    uint32_t challengeId;
    uint32_t declinerId;
    char declinerName[32];
} __attribute__((packed));

// Game Start
struct GameStart {
    uint32_t gameId;
    uint32_t player1Id;
    uint32_t player2Id;
    char player1Name[32];
    char player2Name[32];
    uint8_t boardSize;
    uint32_t currentTurn;
    uint16_t timeLimit;      // Time limit per player in seconds
    uint16_t player1Time;    // Remaining time for player 1
    uint16_t player2Time;    // Remaining time for player 2
} __attribute__((packed));

// Move Request
struct MoveRequest {
    uint32_t gameId;
    uint8_t x;
    uint8_t y;
} __attribute__((packed));

// Move Response
struct MoveResponse {
    uint8_t success;
    uint8_t x;
    uint8_t y;
    uint8_t player;
    uint32_t nextTurn;
    uint16_t player1Time;    // Remaining time for player 1
    uint16_t player2Time;    // Remaining time for player 2
    uint32_t moveNumber;     // Move number in the game
} __attribute__((packed));

// Game Over
struct GameOver {
    uint32_t gameId;
    uint32_t winnerId;
    char winnerName[32];
    int16_t eloChange;
    uint8_t reason;  // 0 = checkmate, 1 = resign, 2 = timeout, 3 = draw
    uint32_t totalMoves;
} __attribute__((packed));

// Player Info
struct PlayerInfo {
    uint32_t userId;
    char username[32];
    uint16_t eloRating;
    uint16_t wins;
    uint16_t losses;
    uint16_t draws;
    uint8_t isOnline;
    uint8_t inGame;
} __attribute__((packed));

// Draw Request/Response
struct DrawRequest {
    uint32_t gameId;
} __attribute__((packed));

// Rematch Request
struct RematchRequest {
    uint32_t lastGameId;
    uint32_t opponentId;
} __attribute__((packed));

// Move Log Entry
struct MoveLogEntry {
    uint32_t moveNumber;
    uint32_t playerId;
    uint8_t x;
    uint8_t y;
    uint32_t timestamp;  // Time when move was made (seconds since game start)
} __attribute__((packed));

// Game Log Response Header
struct GameLogHeader {
    uint32_t gameId;
    uint32_t player1Id;
    uint32_t player2Id;
    char player1Name[32];
    char player2Name[32];
    uint8_t boardSize;
    uint32_t winnerId;
    uint8_t result;  // 0 = player1 win, 1 = player2 win, 2 = draw
    uint32_t totalMoves;
    uint32_t gameDuration;  // In seconds
    uint64_t timestamp;     // Game start timestamp
} __attribute__((packed));

// Game History Entry (simplified for list)
struct GameHistoryEntry {
    uint32_t gameId;
    uint32_t opponentId;
    char opponentName[32];
    uint8_t result;  // 0 = win, 1 = loss, 2 = draw
    int16_t eloChange;
    uint64_t timestamp;
} __attribute__((packed));

// Time Update
struct TimeUpdate {
    uint32_t gameId;
    uint16_t player1Time;
    uint16_t player2Time;
} __attribute__((packed));

// Resign Request
struct ResignRequest {
    uint32_t gameId;
} __attribute__((packed));

#endif
