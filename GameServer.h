#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <SFML/Network.hpp>
#include <vector>
#include <mutex>

enum GameMessageType : unsigned char {
    JOIN_GAME = 0x01, PLACE_TOKEN = 0x02, START_GAME = 0x03, GAME_OVER = 0x04
};

enum Token : unsigned char {
    NOUGHTS = 0x01, CROSSES = 0x02
};

class GameServer {
public:
    GameServer(unsigned short tcp_port);
    void tcp_start();

private:
    unsigned short m_tcp_port;
    unsigned char m_board[3][3] = { {0,0,0}, {0,0,0}, {0,0,0} };
    std::mutex m_board_mutex;
    unsigned short m_player_count { 0 };
    unsigned short m_turns_played { 0 };
    
    std::vector<sf::TcpSocket*> m_clients;
    std::mutex m_clients_mutex;

    void handle_client(sf::TcpSocket* client, unsigned short player_num);
    bool broadcast_message(const char *buffer, sf::TcpSocket* sender);
    constexpr size_t message_size(const char messageType);
    void debug_message(const char *buf);
    bool send_start_game_to_clients();
    bool checkForWinner(unsigned char token);
    bool send_game_over_to_clients(unsigned char winnerToken);
};

#endif