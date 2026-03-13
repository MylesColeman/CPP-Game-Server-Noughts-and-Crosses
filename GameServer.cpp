#include "GameServer.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

// Note: This is compiled with SFML 2.6.2 in mind.
// It would work similarly with slightly older versions of SFML.
// A thourough rework is necessary for SFML 3.0.

GameServer::GameServer(unsigned short tcp_port) : m_tcp_port(tcp_port) {}

// Binds to a port and then loops around.  For every client that connects,
// we start a new thread receiving their messages.
void GameServer::tcp_start()
{
    // BINDING
    sf::TcpListener listener;
    sf::Socket::Status status = listener.listen(m_tcp_port, sf::IpAddress::Any);
    if (status != sf::Socket::Status::Done)
    {
        std::cerr << "Error binding listener to port" << std::endl;
        return;
    }

    std::cout << "TCP Server is listening to port "
        << m_tcp_port
        << ", waiting for connections..."
        << std::endl;

    while (true)
    {
        // ACCEPTING
        if(m_player_count < 2)
        {
            sf::TcpSocket* client = new sf::TcpSocket;
            status = listener.accept(*client);
            if (status != sf::Socket::Status::Done)
            {
                delete client;
            } else {
                {
                    std::lock_guard<std::mutex> lock(m_clients_mutex);
                    m_clients.push_back(client);
                }
                std::cout << "New client connected: "
                    << client->getRemoteAddress()
                    << std::endl;
                    
                m_player_count++;

                std::thread(&GameServer::handle_client, this, client, m_player_count).detach();

                if(m_player_count == 2)
                {
                    // --------------------------------------------------------------
                    // Slight pause to ensure the all threads have started
                    // --------------------------------------------------------------
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));

                    // As we no have two players, let's start the game:
                    if(!send_start_game_to_clients())
                    {
                        std::cerr << "Unable to start the game, one or both players didn't receive the START_GAME message" << std::endl;    
                    }
                }
            }
        }
    }
    // No need to call close of the listener.
    // The connection is closed automatically when the listener object is out of scope.
}

// Loop around, receive messages from client and send them to all
// the other connected clients.
void GameServer::handle_client(sf::TcpSocket* client, unsigned short player_num)
{
    sf::Socket::Status status;

    if(player_num == 1) {
        std::cout << "Player " << player_num << " is NOUGHTS" << std::endl;
        char buffer[2] = {
            GameMessageType::JOIN_GAME,
            Token::NOUGHTS
        };
        
        status = client->send(buffer, message_size(GameMessageType::JOIN_GAME));

        if (status != sf::Socket::Status::Done)
        {
            std::cerr << "Error sending JOIN_GAME to player 1" << std::endl;
            return;
        }
    }
    else if(player_num == 2) {
        std::cout << "Player " << player_num << " is CROSSES" << std::endl;
        char buffer[2] = {
            GameMessageType::JOIN_GAME,
            Token::CROSSES
        };
        
        status = client->send(buffer, message_size(GameMessageType::JOIN_GAME));

        if (status != sf::Socket::Status::Done)
        {
            std::cerr << "Error sending JOIN_GAME to player 2" << std::endl;
            return;
        }            
    }
    else {
        return; // No more players please!!!
    }
        

    while (true)
    {
        // RECEIVING
        char payload[1024];
        std::memset(payload, 0, 1024);
        size_t received;
        sf::Socket::Status status = client->receive(payload, 1024, received);
        if (status != sf::Socket::Status::Done)
        {
            std::cerr << "Error receiving message from client" << std::endl;
            break;
        } else {
            // Actually, there is no need to print the message if the message is not a string
            debug_message(payload);

            if (payload[0] == GameMessageType::PLACE_TOKEN)
            {
                const unsigned char *rowPtr { (unsigned char* )payload + 1 };
                const unsigned char *colPtr { (unsigned char* )payload + 1 + sizeof(int) };
                unsigned int row = be32toh(*((unsigned int *) rowPtr));
                unsigned int col = be32toh(*((unsigned int *) colPtr));

                unsigned char currentToken = (player_num == 1) ? Token::NOUGHTS : Token::CROSSES;

                {
                    std::lock_guard<std::mutex> lock(m_board_mutex);

                    if (row < 3 && col < 3 && m_board[row][col] == 0)
                    {
                        m_board[row][col] = currentToken;
                        m_turns_played++;

                        if (checkForWinner(currentToken))
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            send_game_over_to_clients(currentToken);
                        }
                        else if (m_turns_played == 9)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            send_game_over_to_clients(0);
                        }
                    }
                    else
                        std::cout << "Invalid move attempted by Player " << player_num << std::endl;
                }
                broadcast_message(payload, client);
            }
        }
    }

    // Everything that follows only makes sense if we have a graceful way to exiting the loop.
    // Remove the client from the list when done
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client),
                m_clients.end());
    }
    delete client;
}

// Sends `message` from `sender` to all the other connected clients
bool GameServer::broadcast_message(const char *buffer, sf::TcpSocket* sender)
{
    size_t msgSize { message_size(buffer[0]) };

    std::lock_guard<std::mutex> lock(m_clients_mutex);
    for (auto& client : m_clients)
    {
        if (client != sender)
        {
            // SENDING
            sf::Socket::Status status = client->send(buffer, msgSize) ;
            if (status != sf::Socket::Status::Done)
            {
                std::cerr << "Error sending message to client" << std::endl;

                return false;
            }
        }
    }

    return true;
}

constexpr size_t GameServer::message_size(const char messageType)
{
    switch(messageType) {
        case JOIN_GAME:     return 2;
        case PLACE_TOKEN:   return sizeof(int) * 2 + 2;
        case START_GAME:    return 1;
        case GAME_OVER:     return 2;
        default: return 0;
    }
}

void GameServer::debug_message(const char *buf)
{
    const unsigned char msgType = buf[0];

    switch(msgType) {
        case JOIN_GAME: {
            std::cout << "Player Joined The Game" << std::endl;
            break;
        }
        case PLACE_TOKEN: {
            const unsigned char *row { (unsigned char* )buf + 1 };
            const unsigned char *col { (unsigned char* )buf + 1 + sizeof(int) };
            unsigned int rowI = be32toh(*((unsigned int *) row));
            unsigned int colI = be32toh(*((unsigned int *) col));
            std::cout << "Player Placed A Token: (" << rowI << ", " << colI << ")" << std::endl;

            break;
        }
    }
}

bool GameServer::send_start_game_to_clients()
{
    char buf[1] = { START_GAME };
    
    std::cout << "STARTING THE GAME" << std::endl;

    return broadcast_message(buf, nullptr);
}

bool GameServer::checkForWinner(unsigned char token) {
    for (int i = 0; i < 3; i++) {
        if (m_board[i][0] == token && m_board[i][1] == token && m_board[i][2] == token) return true;
        if (m_board[0][i] == token && m_board[1][i] == token && m_board[2][i] == token) return true;
    }

    if (m_board[0][0] == token && m_board[1][1] == token && m_board[2][2] == token) return true;
    if (m_board[0][2] == token && m_board[1][1] == token && m_board[2][0] == token) return true;

    return false;
}

bool GameServer::send_game_over_to_clients(unsigned char winnerToken)
{
    char buf[2] = { static_cast<char>(GAME_OVER), static_cast<char>(winnerToken) };
    
    std::cout << "GAME OVER! Winner: " << (int)winnerToken << std::endl;

    return broadcast_message(buf, nullptr);
}