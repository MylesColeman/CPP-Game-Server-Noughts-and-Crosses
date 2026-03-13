#include <SFML/Network.hpp>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// TODO: move `GameServer` into its own files (h/cpp).
// Note: This is compiled with SFML 2.6.2 in mind.
// It would work similarly with slightly older versions of SFML.
// A thourough rework is necessary for SFML 3.0.

enum GameMessageType : unsigned char {
    JOIN_GAME = 0x01, PLACE_TOKEN = 0x02
};

enum Token : unsigned char {
    NOUGHTS = 0x01, CROSSES = 0x02
};

class GameServer {
public:
    GameServer(unsigned short tcp_port) :
        m_tcp_port(tcp_port) {}

    // Binds to a port and then loops around.  For every client that connects,
    // we start a new thread receiving their messages.
    void tcp_start()
    {
        // BINDING
        sf::TcpListener listener;
        sf::Socket::Status status = listener.listen(m_tcp_port, sf::IpAddress("A.B.C.D"));
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
                    }
                }
            }
        }
        // No need to call close of the listener.
        // The connection is closed automatically when the listener object is out of scope.
    }


private:
    unsigned short m_tcp_port;
    unsigned short m_player_count { 0 };
    
    std::vector<sf::TcpSocket*> m_clients;
    std::mutex m_clients_mutex;

    // Loop around, receive messages from client and send them to all
    // the other connected clients.
    void handle_client(sf::TcpSocket* client, unsigned short player_num)
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

                broadcast_message(payload, client);
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
    bool broadcast_message(const char *buffer, sf::TcpSocket* sender)
    {
        size_t msgSize { message_size(buffer[0]) };

        // You might want to validate the message before you send it.
        // A few reasons for that:
        // 1. Make sure the message makes sense in the game.
        // 2. Make sure the sender is not cheating.
        // 3. First need to synchronise the players inputs (usually done in Lockstep).
        // 4. Compensate for latency and perform rollbacks (usually done in Ded Reckoning).
        // 5. Delay the sending of messages to make the game fairer wrt high ping players.
        // This is where you can write the authoritative part of the server.
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

    constexpr size_t message_size(const char messageType)
    {
        switch(messageType) {
            case JOIN_GAME:     return 2;
            case PLACE_TOKEN:   return sizeof(int) * 2 + 2;
            default: return 0;
        }
    }

    void debug_message(const char *buf)
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
};

int main()
{
    GameServer server(4300);
    
    server.tcp_start();
    
    return 0;
}
