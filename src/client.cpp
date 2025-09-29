#include <EasyNet/EasyNetClient.hpp>
#include <RaylibRetainedGUI/RaylibRetainedGUI.hpp>

#include "shared.hpp"

#include <memory>
#include <iostream>

bool received_game = false;
GameState first_received_game{};
uint32_t last_received_tick = 0;
uint32_t first_received_game_tick = 0;

GameState game_state{};

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
            BeginDrawing();
            DrawText(std::to_string(tick).c_str(), 100, 100, 64, WHITE);
            DrawText(std::to_string(client->GetPeer()->roundTripTime).c_str(), 100, 200, 64, WHITE);
            
            Color color = GREEN;
            game_manager.Draw(game_state, &color);
            color = RED;
            //game_manager.Draw(local_game_state, &color);            

            ClearBackground(DARKGRAY);
            EndDrawing();

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
            
            game_state = game_manager.ApplyEvents(game_state, tick, tick+1);
            //local_game_state = game_manager.ApplyEvents(local_game_state, tick, tick+1);
            if (GetTime() > 23) {
                game_manager.OutputHistory();

                GameState state = {};
                char buffer[MAX_STRING_LENGTH];
                SerializeGameState(game_manager.ApplyEvents(first_received_game, first_received_game_tick, last_received_tick), buffer, MAX_STRING_LENGTH);
                std::cout << last_received_tick << std::endl;
                std::cout << std::endl << buffer << std::endl;
                CloseWindow();
            }
            tick += 1;
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

    if (!client->ConnectToServer("127.0.0.1", 7777)) {
        client->RequestConnectToServer("45.159.79.84", 7777);
    }    
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
        GameStatePacketData data = ExtractData<GameStatePacketData>(event.packet);
        auto rec_state = DeserializeGameState(data.text, MAX_STRING_LENGTH);
        if (!received_game) {
            first_received_game = rec_state;
            first_received_game_tick = data.tick;
            received_game = true;
        }
        game_state = game_manager.ApplyEvents(rec_state, data.tick, tick);
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
