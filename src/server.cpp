#include <EasyNet/EasyNetServer.hpp>
#include <memory>
#include "shared.hpp"
#include <thread>

bool sent_game = false;
GameState first_game_sent{};
uint32_t first_sent_game_tick = 0;
uint32_t last_sent_tick = 0;

GameState game_state{};
GameState old_game_state{};
Game game_manager{};
uint32_t tick = 0;

std::unique_ptr<EasyNetServer> server;
bool running = true;

void OnConnect(ENetEvent event);
void OnDisconnect(ENetEvent event);
void OnRecieve(ENetEvent event);
void UpdateServer();

#define WINDOW_VISUALIZATION 0

int main(){
    if (WINDOW_VISUALIZATION) {
        InitWindow(1000, 1000, "Server");
        SetWindowState(FLAG_WINDOW_TOPMOST);
        SetTargetFPS(iters_per_sec);
    }    

    std::cout << "Server running" << std::endl;
    EasyNetInit();
    server = std::make_unique<EasyNetServer>();
    server->CreateServer();
    server->SetOnConnect(OnConnect);
    server->SetOnDisconnect(OnDisconnect);
    server->SetOnReceive(OnRecieve);
    
    if (WINDOW_VISUALIZATION) {
        while (!WindowShouldClose() && running) {
            UpdateServer();
            tick++;        

            BeginDrawing();
            ClearBackground(GRAY);
            Color color = BLUE;
            game_manager.Draw(game_state, &color);
            EndDrawing();
        }
    }
    else {
        auto next_tick = std::chrono::steady_clock::now();
        while (running) {
            auto now = std::chrono::steady_clock::now();

            while (now >= next_tick) {
                UpdateServer();
                tick++;

                next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(dt)
                );
            }
            std::this_thread::sleep_until(next_tick);
        }
    }
    
    CloseWindow();
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

    running = false;
    game_manager.OutputHistory();

    char buffer[MAX_STRING_LENGTH];
    SerializeGameState(game_manager.ApplyEvents(first_game_sent, first_sent_game_tick, last_sent_tick), buffer, MAX_STRING_LENGTH);
    std::cout << last_sent_tick << std::endl;
    std::cout << std::endl << buffer << std::endl;
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

void UpdateServer() {
    server->Update();

    constexpr uint32_t tick_period = iters_per_sec/10; // broadcast game state every 100 ms
    constexpr uint32_t receive_tick_period = iters_per_sec; // allow late received events
    constexpr uint32_t send_tick_period = iters_per_sec*2; // sync client's tick with server's tick

    if (tick % tick_period == 0) {
        uint32_t current_tick = tick-receive_tick_period;

        uint32_t previous_tick = current_tick - tick_period;
        uint32_t current_old_tick = current_tick - receive_tick_period;
        uint32_t previous_old_tick = previous_tick - receive_tick_period;

        old_game_state = game_manager.ApplyEvents(old_game_state, previous_old_tick, current_old_tick);
        game_state = game_manager.ApplyEvents(old_game_state, current_old_tick, current_tick);

        char buffer[MAX_STRING_LENGTH];
        SerializeGameState(game_state, buffer, sizeof(buffer));
        
        GameStatePacketData data;
        std::strncpy(data.text, buffer, sizeof(buffer));
        data.tick = current_tick;

        ENetPacket* packet = CreatePacket<GameStatePacketData>(MSG_GAME_STATE, data);
        server->Broadcast(packet);     
        if (!sent_game) {
            first_game_sent = game_state;
            sent_game = true;            
            first_sent_game_tick = tick;
        }   
    }

    if (tick % send_tick_period == 0) {
        server->Broadcast(CreatePacket<uint32_t>(MSG_GAME_TICK, tick));        
        last_sent_tick = tick;
    }
}
