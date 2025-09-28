#include <EasyNet/EasyNetClient.hpp>
#include <RaylibRetainedGUI/RaylibRetainedGUI.hpp>

#include "shared.hpp"

#include <memory>
#include <iostream>

GameState game_state{};
Game game_manager{};
uint32_t tick = 0;

std::shared_ptr<EasyNetClient> client;

std::shared_ptr<UIScreen> screen;
std::shared_ptr<UIBar> bar;
std::shared_ptr<UIFuncButton> connect_button;

std::string server_ip = "127.0.0.1";
int server_port = 7777;

bool connected = false;

void Init();
void OnReceive(ENetEvent event);

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
            game_manager.Draw(game_state);
            ClearBackground(DARKGRAY);
            EndDrawing();

            PlayerInputPacketData data;
            data.input.right = IsKeyDown(KEY_D);
            data.input.left = IsKeyDown(KEY_A);
            data.input.up = IsKeyPressed(KEY_W);
            tick += 1;
            data.tick = tick;
            client->SendPacket(CreatePacket<PlayerInputPacketData>(MSG_PLAYER_INPUT, data));
        }
    }
    return 0;
}

void Init() {
    EasyNetInit();
    InitWindow(1000, 1000, "Multiplayer");
    SetWindowState(FLAG_WINDOW_TOPMOST);
    SetTargetFPS(dt);

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

    //client->RequestConnectToServer("127.0.0.1", 7777);
}

void OnReceive(ENetEvent event) {
    MessageType msgType = static_cast<MessageType>(event.packet->data[0]);
    switch (msgType) {
    case MSG_GAME_STATE:
        {
        GameStatePacketData data = ExtractData<GameStatePacketData>(event.packet);
        game_state = DeserializeGameState(data.text, MAX_STRING_LENGTH);
        tick = data.tick;
        }
        break;

    default:
        break;
    }
}
