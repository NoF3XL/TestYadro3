#include <iostream>
#include <string>
#include <thread>
#include <cmath>
#include <cstring>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/json.hpp"
#include "../common/common.h"

using namespace std;

using json = nlohmann::json;

class AccelerometerClientB {
private:
    int sock_fd;
    string server_ip;
    int server_port;
    atomic<bool> running;
    thread receive_thread;
    
    float calculate_module(float x, float y, float z) {
        return sqrt(x*x + y*y + z*z);
    }
    
    bool connect_to_server() {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            cerr << "[" << get_current_time_str() << "] Ошибка создания сокета" << endl;
            return false;
        }
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            cerr << "[" << get_current_time_str() << "] Неверный IP адрес" << endl;
            return false;
        }
        
        if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "[" << get_current_time_str() << "] Ошибка подключения: " 
                      << strerror(errno) << endl;
            return false;
        }
        
        cout << "[" << get_current_time_str() 
                 << "] Подключен к серверу " << server_ip << ":" << server_port << endl;
        
        json handshake = {{"role", "module_calculator"}};
        string msg = handshake.dump() + "\n";
        send(sock_fd, msg.c_str(), msg.length(), 0);
        
        return true;
    }
    
    bool send_json(const json& data) {
        string msg = data.dump() + "\n";
        int sent = send(sock_fd, msg.c_str(), msg.length(), 0);
        if (sent < 0) {
            cerr << "[" << get_current_time_str() 
                     << "] Ошибка отправки: " << strerror(errno) << endl;
            return false;
        }
        return true;
    }
    
    bool recv_json(json& data) {
        string buffer;
        char ch;
        
        while (true) {
            int n = recv(sock_fd, &ch, 1, 0);
            if (n <= 0) return false;
            
            if (ch == '\n') break;
            buffer += ch;
        }
        
        try {
            data = json::parse(buffer);
            return true;
        } catch (const exception& e) {
            cerr << "[" << get_current_time_str() 
                     << "] JSON parse error: " << e.what() << endl;
            return false;
        }
    }
    
    void process_loop() {
        cout << "[" << get_current_time_str() 
                 << "] Начата обработка данных" << endl;
        
        while (running) {
            json data;
            if (!recv_json(data)) {
                cout << "[" << get_current_time_str() 
                         << "] Соединение с сервером разорвано" << endl;
                break;
            }
            
            if (!data.contains("timestamp") || !data.contains("x") || 
                !data.contains("y") || !data.contains("z")) {
                cerr << "[" << get_current_time_str() 
                         << "] Неверный формат данных" << endl;
                continue;
            }
            
            int64_t timestamp = data["timestamp"];
            float x = data["x"];
            float y = data["y"];
            float z = data["z"];
            
            float module = calculate_module(x, y, z);
            
            cout << "[" << get_current_time_str() 
                     << "] Получены данные: x=" << x << " y=" << y << " z=" << z
                     << " -> модуль=" << module << endl;
            
            json result = {
                {"timestamp", timestamp},
                {"module", round(module * 10000.0f) / 10000.0f}
            };
            
            if (!send_json(result)) {
                break;
            }
            
            cout << "[" << get_current_time_str() 
                     << "] Результат отправлен серверу" << endl;
        }
    }
    
public:
    AccelerometerClientB(const string& ip, int port) 
        : server_ip(ip), server_port(port), running(true) {}
    
    void run() {
        if (!connect_to_server()) {
            return;
        }
        
        receive_thread = thread(&AccelerometerClientB::process_loop, this);
        receive_thread.join();
    }
    
    ~AccelerometerClientB() {
        running = false;
        if (sock_fd >= 0) {
            close(sock_fd);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Использование: " << argv[0] << " <сервер_IP> <порт>" << endl;
        return 1;
    }
    
    string server_ip = argv[1];
    int port = stoi(argv[2]);
    
    cout << "=== Клиент B (вычислитель модуля ускорения) ===" << endl;
    cout << "Сервер: " << server_ip << ":" << port << endl;
    
    AccelerometerClientB client(server_ip, port);
    client.run();
    
    return 0;
}