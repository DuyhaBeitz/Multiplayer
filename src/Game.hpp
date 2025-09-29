#pragma once

#include "GameBase.hpp"
#include <variant>
#include <raylib.h>
#include <raymath.h>

#include <nlohmann/json.hpp>

constexpr int iters_per_sec = 60;
constexpr double dt = 1.f/iters_per_sec;
constexpr double gravity = 40;
constexpr double floor_lvl = 500;
constexpr double hor_speed = 60;
constexpr double jump_impulse = 30;


enum EventId {
    EV_PLAYER_JOIN = 0,
    EV_PLAYER_LEAVE,
    EV_PLAYER_INPUT,
};

// Event data
struct PlayerJoin {};
struct PlayerLeave {};
struct PlayerInput {
    bool right;
    bool left;
    bool up;

    float GetX() { return right - left; }
};

struct GameEvent {
    EventId event_id; // for sending over the net
    std::variant<std::monostate, PlayerJoin, PlayerLeave, PlayerInput> data;
};

struct PlayerState {
    Vector2 position;
    Vector2 velocity;
};

struct GameState {
    std::map<uint32_t, PlayerState> players;
};

struct GameDrawData {
    Color color;
};

#define MAX_STRING_LENGTH 1024

inline size_t SerializeGameState(const GameState& state, char* buffer, size_t max_len) {
    nlohmann::json j;

    for (const auto& [id, player] : state.players) {
        j["players"][std::to_string(id)] = {
            {"px", player.position.x},
            {"py", player.position.y},
            {"vx", player.velocity.x},
            {"vy", player.velocity.y}
        };
    }

    std::string s = j.dump();
    size_t len = std::min(max_len - 1, s.size());  // leave room for '\0'
    std::memcpy(buffer, s.data(), len);
    buffer[len] = '\0'; // null-terminate for safety
    return len;         // return actual length
}

inline GameState DeserializeGameState(const char* buffer, size_t len) {
    nlohmann::json j = nlohmann::json::parse(std::string(buffer, len));
    GameState state;

    for (auto& [id_str, player_json] : j["players"].items()) {
        uint32_t id = static_cast<uint32_t>(std::stoul(id_str));
        PlayerState ps;
        ps.position.x = player_json["px"].get<float>();
        ps.position.y = player_json["py"].get<float>();
        ps.velocity.x = player_json["vx"].get<float>();
        ps.velocity.y = player_json["vy"].get<float>();
        state.players[id] = ps;
    }

    return state;
}

class Game : public GameBase<GameState, GameEvent, GameDrawData> {
public:
    virtual void ApplyEvent(GameState& state, const GameEvent& event, uint32_t id) {
        switch (event.event_id) {
        case EV_PLAYER_JOIN:
            state.players[id] = PlayerState{Vector2{100, 100}};
            break;

        case EV_PLAYER_LEAVE:
            state.players.erase(id);
            break;

        case EV_PLAYER_INPUT:
            if (std::holds_alternative<PlayerInput>(event.data)) {
                auto input = std::get<PlayerInput>(event.data);
                state.players[id].velocity.x += input.GetX() * dt * hor_speed;
                if (input.up && state.players[id].position.y == floor_lvl) state.players[id].velocity.y -= jump_impulse;
            }
            break; 

        default:
            break;
        }
    }

    virtual void Draw(const GameState& state, GameDrawData data) {
        for (const auto& [id, player] : state.players) {
            DrawCircleV(player.position, 10, data.color);
        }
    }

    virtual void UpdateGameLogic(GameState& state) {
        for (auto& [id, player] : state.players) {
            player.velocity.y += gravity*dt;
            player.position += player.velocity;
            if (player.position.y > floor_lvl) {
                player.position.y = floor_lvl;
            }
            player.velocity *= 0.9;
        }
    }
};