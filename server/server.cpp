#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../common/json.hpp"
#include "../common/common.h"

using namespace std;

using json = nlohmann::json;

class AccelerometerServer {
private:
    int server_fd;
    int client_a_fd = -1;
    int client_b_fd = -1;
    int port;
    bool running;
    
    float last_x = 0, last_y = 0, last_z = 0;
    bool has_last = false;
    
    vector<thread> client_threads;
    
    const float EPSILON = 0.0001f;
    
    bool is_duplicate(float x, float y, float z) {
        if (!has_last) return false;
        
        return (abs(x - last_x) < EPSILON &&
                abs(y - last_y) < EPSILON &&
                abs(z - last_z) < EPSILON);
    }
    
    void update_last(float x, float y, float z) {
        last_x = x;
        last_y = y;
        last_z = z;
        has_last = true;
    }
    
    bool send_json(int fd, const json& data) {
        string msg = data.dump() + "\n";
        int sent = send(fd, msg.c_str(), msg.length(), 0);
        if (sent < 0) {
            cerr << "[" << get_current_time_str() << "] Ошибка отправки: " 
                      << strerror(errno) << endl;
            return false;
        }
        return true;
    }
    
    bool recv_json(int fd, json& data) {
        string buffer;
        char ch;
        
        while (true) {
            int n = recv(fd, &ch, 1, 0);
            if (n <= 0) return false;
            
            if (ch == '\n') break;
            buffer += ch;
        }
        
        try {
            data = json::parse(buffer);
            return true;
        } catch (const exception& e) {
            cerr << "[" << get_current_time_str() << "] JSON parse error: " 
                      << e.what() << endl;
            return false;
        }
    }
    
    void handle_client_a() {
        cout << "[" << get_current_time_str() << "] Начало обработки клиента A" << endl;
        
        while (running && client_a_fd >= 0) {
            json data;
            if (!recv_json(client_a_fd, data)) {
                cout << "[" << get_current_time_str() 
                         << "] Клиент A отключился" << endl;
                close(client_a_fd);
                client_a_fd = -1;
                break;
            }
            
            if (!data.contains("timestamp") || !data.contains("x") || 
                !data.contains("y") || !data.contains("z")) {
                cerr << "[" << get_current_time_str() 
                         << "] Неверный формат от A" << endl;
                continue;
            }
            
            float x = data["x"];
            float y = data["y"];
            float z = data["z"];
            
            if (is_duplicate(x, y, z)) {
                cout << "[" << get_current_time_str() 
                         << "] Дубликат отброшен: x=" << x 
                         << " y=" << y << " z=" << z << endl;
                continue;
            }
            
            update_last(x, y, z);
            
            cout << "[" << get_current_time_str() 
                     << "] Получены данные от A: ts=" << data["timestamp"]
                     << " x=" << x << " y=" << y << " z=" << z << endl;
            
            if (client_b_fd >= 0) {
                if (!send_json(client_b_fd, data)) {
                    cerr << "[" << get_current_time_str() 
                             << "] Ошибка отправки клиенту B" << endl;
                } else {
                    cout << "[" << get_current_time_str() 
                             << "] Данные пересланы клиенту B" << endl;
                }
            } else {
                cout << "[" << get_current_time_str() 
                         << "] Клиент B не подключен, данные не пересланы" << endl;
            }
        }
    }
    
    void handle_client_b() {
        cout << "[" << get_current_time_str() << "] Начало обработки клиента B" << endl;
        
        while (running && client_b_fd >= 0) {
            json result;
            if (!recv_json(client_b_fd, result)) {
                cout << "[" << get_current_time_str() 
                         << "] Клиент B отключился" << endl;
                close(client_b_fd);
                client_b_fd = -1;
                break;
            }
            
            if (!result.contains("timestamp") || !result.contains("module")) {
                continue;
            }
            
            cout << "[" << get_current_time_str() 
                     << "] Получен модуль от B: ts=" << result["timestamp"]
                     << " module=" << result["module"] << endl;
            
            if (client_a_fd >= 0) {
                if (!send_json(client_a_fd, result)) {
                    cerr << "[" << get_current_time_str() 
                             << "] Ошибка отправки клиенту A" << endl;
                } else {
                    cout << "[" << get_current_time_str() 
                             << "] Модуль переслан клиенту A" << endl;
                }
            }
        }
    }
    
public:
    AccelerometerServer(int port) : port(port), running(true) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw runtime_error("Не удалось создать сокет");
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd);
            throw runtime_error("Не удалось привязать порт");
        }
        
        if (listen(server_fd, 3) < 0) {
            close(server_fd);
            throw runtime_error("Ошибка listen");
        }
    }
    
    void run() {
        cout << "[" << get_current_time_str() 
                  << "] Сервер запущен на порту " << port << endl;
        
        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                if (running) {
                    cerr << "[" << get_current_time_str() 
                             << "] Ошибка accept: " << strerror(errno) << endl;
                }
                continue;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            cout << "[" << get_current_time_str() 
                     << "] Новое подключение от " << client_ip 
                     << ":" << ntohs(client_addr.sin_port) << endl;
            
            json handshake;
            if (recv_json(client_fd, handshake)) {
                string role = handshake.value("role", "");
                if (role == "accelerometer" && client_a_fd < 0) {
                    client_a_fd = client_fd;
                    client_threads.emplace_back(&AccelerometerServer::handle_client_a, this);
                    cout << "[" << get_current_time_str() 
                             << "] Клиент A (акселерометр) подключен" << endl;
                } else if (role == "module_calculator" && client_b_fd < 0) {
                    client_b_fd = client_fd;
                    client_threads.emplace_back(&AccelerometerServer::handle_client_b, this);
                    cout << "[" << get_current_time_str() 
                             << "] Клиент B (вычислитель модуля) подключен" << endl;
                } else {
                    cout << "[" << get_current_time_str() 
                             << "] Неизвестный клиент или дубликат, отключение" << endl;
                    close(client_fd);
                }
            } else {
                cout << "[" << get_current_time_str() 
                         << "] Ошибка чтения первого сообщения, отключение" << endl;
                close(client_fd);
            }
        }
    }
    
    ~AccelerometerServer() {
        running = false;
        close(server_fd);
        if (client_a_fd >= 0) close(client_a_fd);
        if (client_b_fd >= 0) close(client_b_fd);
        
        for (auto& t : client_threads) {
            if (t.joinable()) t.join();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Использование: " << argv[0] << " <порт>" << endl;
        return 1;
    }
    
    int port = stoi(argv[1]);
    
    try {
        AccelerometerServer server(port);
        server.run();
    } catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}