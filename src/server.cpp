#include <EasyNet/EasyNetServer.hpp>
#include <memory>
#include "shared.hpp"
#include <thread>

GameState game_state{};
Game game_manager{};
uint32_t tick = 0;

std::unique_ptr<EasyNetServer> server;
bool running = true;

void OnConnect(ENetEvent event);
void OnDisconnect(ENetEvent event);
void OnRecieve(ENetEvent event);
void UpdateTick();

int main(){
    std::cout << "Server running" << std::endl;
    EasyNetInit();
    server = std::make_unique<EasyNetServer>();
    server->CreateServer();
    server->SetOnConnect(OnConnect);
    server->SetOnDisconnect(OnDisconnect);
    server->SetOnReceive(OnRecieve);
    
    auto next_tick = std::chrono::steady_clock::now();
    while (running) {
        auto now = std::chrono::steady_clock::now();
        int k = 1;
        while (now >= next_tick) {
            for (int i = 0; i < k; i++) {
                UpdateTick();
                tick++;
            }
            next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(dt*k)
            );
        }
        std::this_thread::sleep_until(next_tick);
        std::cout << tick << std::endl;
    }
    return 0;
}

void OnConnect(ENetEvent event) {
    GameEvent game_event;
    game_event.event_id = EV_PLAYER_JOIN;
    uint32_t id = enet_peer_get_id(event.peer);
    game_manager.AddEvent(game_event, id, tick);
    server->SendTo(id, CreatePacket<uint32_t>(MSG_GAME_TICK, tick));
    server->SendTo(id, CreatePacket<uint32_t>(MSG_PLAYER_ID, id));
}

void OnDisconnect(ENetEvent event) {
    GameEvent game_event;
    game_event.event_id = EV_PLAYER_LEAVE;
    uint32_t id = enet_peer_get_id(event.peer);
    game_manager.AddEvent(game_event, id, tick);
}

void OnRecieve(ENetEvent event)
{
    MessageType msgType = static_cast<MessageType>(event.packet->data[0]);
    switch (msgType) {
    case MSG_PLAYER_INPUT:
        {
        PlayerInputPacketData recieved = ExtractData<PlayerInputPacketData>(event.packet);
        uint32_t id = enet_peer_get_id(event.peer);

        GameEvent game_event;
        game_event.event_id = EV_PLAYER_INPUT;
        game_event.data = recieved.input;

        game_manager.AddEvent(game_event, id, recieved.tick);
        }
        break;

    default:
        break;
    }
}

void UpdateTick() {
    server->Update();

    game_state = game_manager.ApplyEventsAsOneTick(game_state);

    char buffer[MAX_STRING_LENGTH];
    SerializeGameState(game_state, buffer, sizeof(buffer));
    
    GameStatePacketData data;
    std::strncpy(data.text, buffer, sizeof(buffer));
    data.tick = tick;

    ENetPacket* packet = CreatePacket<GameStatePacketData>(MSG_GAME_STATE, data);
    server->Broadcast(packet);
}
