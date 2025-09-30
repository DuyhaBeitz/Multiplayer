#include <EasyNet/EasyNetClient.hpp>
#include <RaylibRetainedGUI/RaylibRetainedGUI.hpp>

#include "shared.hpp"

#include <memory>
#include <iostream>

// debug
bool received_game = false;
GameState first_received_game{};
uint32_t last_received_tick = 0;
uint32_t first_received_game_tick = 0;


uint32_t ticks_since_last_recieved_game = 0;
uint32_t prev_last_received_game_tick = 0;
GameState prev_last_received_game{};
uint32_t last_received_game_tick = 0;
GameState last_received_game{};
GameState others_game_state{};
GameState self_game_state{};

Game game_manager{};
uint32_t tick = 0;
uint32_t id = 0;

std::shared_ptr<EasyNetClient> client;

std::shared_ptr<UIScreen> screen;
std::shared_ptr<UIBar> bar;
std::shared_ptr<UIFuncButton> connect_button;

std::string server_ip = "127.0.0.1";
int server_port = 7777;

bool connected = false;

void Init();
void OnReceive(ENetEvent event);
uint32_t CalculateTickWinthPing(uint32_t tick);

int main() {
    Init();

    while (!WindowShouldClose()) {
        client->Update();

        if (!connected) {
            screen->Update(nullptr);
            BeginDrawing();
            ClearBackground(LIGHTGRAY);
            screen->Draw();
            EndDrawing();
        }
        else {
            PlayerInput input;
            
            input.right = IsKeyDown(KEY_D);
            input.left = IsKeyDown(KEY_A);
            input.up = IsKeyPressed(KEY_W);

            if (input.GetX() != 0 || input.up) {
                GameEvent event;
                event.event_id = EV_PLAYER_INPUT;
                event.data = input;
                game_manager.AddEvent(event, id, tick);

                PlayerInputPacketData data;
                data.input = input;
                data.tick = tick;
                client->SendPacket(CreatePacket<PlayerInputPacketData>(MSG_PLAYER_INPUT, data, ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT));
            }
            
            self_game_state = game_manager.ApplyEvents(self_game_state, tick, tick+1);
            float alpha = float(ticks_since_last_recieved_game) / float(last_received_game_tick-prev_last_received_game_tick);
            others_game_state = game_manager.Lerp(prev_last_received_game, last_received_game, alpha, &id);

            // if (GetTime() > 4) {
            //     game_manager.OutputHistory();

            //     GameState state = {};
            //     char buffer[MAX_STRING_LENGTH];
            //     SerializeGameState(game_manager.ApplyEvents(first_received_game, first_received_game_tick, last_received_tick), buffer, MAX_STRING_LENGTH);
            //     std::cout << last_received_tick << std::endl;
            //     std::cout << std::endl << buffer << std::endl;
            //     CloseWindow();
            // }
            BeginDrawing();
            DrawText(std::to_string(tick).c_str(), 100, 100, 64, WHITE);
            DrawText(("roundtrip: " + std::to_string(client->GetPeer()->roundTripTime) + "ms").c_str(), 100, 200, 64, WHITE);
            
            DrawingData drawing_data = {id, true, GREEN};
            game_manager.Draw(self_game_state, &drawing_data);
            drawing_data = {id, false, RED};
            game_manager.Draw(others_game_state, &drawing_data);

            ClearBackground(DARKGRAY);
            EndDrawing();

            tick++;
            ticks_since_last_recieved_game++;
        }
    }
    client->RequestDisconnectFromServer();
    client->Update();
    return 0;
}

void Init() {
    EasyNetInit();
    InitWindow(1000, 1000, "Multiplayer");
    SetWindowState(FLAG_WINDOW_TOPMOST);
    SetTargetFPS(iters_per_sec);

    client = std::make_shared<EasyNetClient>();
    client->CreateClient();

    /********** UI **********/
    screen = std::make_shared<UIScreen>();
    bar = std::make_shared<UIBar>(CenteredRect(0.9, 0.5));
    Rectangle rect = SizeRect(1, 0.33);

    auto ip_text = std::make_shared<UIText>("IP  ");    
    auto server_ip_button = std::make_shared<UIStringButton>(&server_ip, rect);
    auto split_ip = std::make_shared<UISplit>(ip_text, server_ip_button, 0.3, rect);

    auto port_text = std::make_shared<UIText>("PORT");
    auto server_port_button = std::make_shared<UIIntButton>(&server_port, rect);
    auto split_port = std::make_shared<UISplit>(port_text, server_port_button, 0.3, rect);

    connect_button = std::make_shared<UIFuncButton>("Connect", rect);

    bar->AddChild(split_ip);
    bar->AddChild(split_port);
    bar->AddChild(connect_button);
    
    screen->AddChild(bar);

    /********** LOGIC **********/
    connect_button->BindOnReleased([](){
            client->RequestConnectToServer(server_ip, server_port);
        }
    );
    int a = 1;
    client->SetOnConnect([](ENetEvent){
        connected = true;
    });
    client->SetOnDisconnect([](ENetEvent){
        connected = false;
    });
    client->SetOnReceive(OnReceive);

    client->RequestConnectToServer("45.159.79.84", 7777);
    //client->RequestConnectToServer("127.0.0.1", 7777);
    // if (!client->ConnectToServer("127.0.0.1", 7777)) {
    //     client->RequestConnectToServer("45.159.79.84", 7777);
    // }    
}

void OnReceive(ENetEvent event) {
    MessageType msgType = static_cast<MessageType>(event.packet->data[0]);
    switch (msgType) {
    case MSG_GAME_TICK:
        tick = CalculateTickWinthPing(ExtractData<uint32_t>(event.packet));
        last_received_tick = tick;
        break;

    case MSG_PLAYER_ID:
        id = ExtractData<uint32_t>(event.packet);
        break;
        
    case MSG_GAME_STATE:
        {
        ticks_since_last_recieved_game = 0;
        prev_last_received_game = last_received_game;
        prev_last_received_game_tick = last_received_game_tick;

        GameStatePacketData data = ExtractData<GameStatePacketData>(event.packet);
        auto rec_state = DeserializeGameState(data.text, MAX_STRING_LENGTH);
        self_game_state = game_manager.ApplyEvents(rec_state, data.tick, tick-1);
        
        last_received_game = rec_state;
        last_received_game_tick = data.tick;

        if (!received_game) {
            first_received_game = rec_state;
            first_received_game_tick = data.tick;
            received_game = true;
        }        
        }
        break;

    default:
        break;
    }
}

uint32_t CalculateTickWinthPing(uint32_t tick) {
    float delta_sec = client->GetPeer()->roundTripTime / 2 / 1000;
    uint32_t delta_tick = delta_sec * iters_per_sec;
    return tick + delta_tick;
}
