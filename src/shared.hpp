#pragma once

#include "Game.hpp"


constexpr MessageType MSG_PLAYER_INPUT = MSG_USER_BASE;
constexpr MessageType MSG_GAME_STATE = MSG_USER_BASE+1;
constexpr MessageType MSG_GAME_TICK = MSG_USER_BASE+2;
constexpr MessageType MSG_PLAYER_ID = MSG_USER_BASE+3;

struct PlayerInputPacketData {
    PlayerInput input;
    uint32_t tick;

    PlayerInputPacketData() = default;
};

struct GameStatePacketData {
    char text[MAX_STRING_LENGTH] = {};
    uint32_t tick;

    GameStatePacketData() = default;
};