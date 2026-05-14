#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <atomic>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../common/json.hpp"
#include "../common/common.h"

using namespace std;

using json = nlohmann::json;

class AccelerometerClientA {
private:
    int sock_fd;
    string server_ip;
    int server_port;
    atomic<bool> running;
    ofstream log_file;
    thread send_thread;
    thread receive_thread;
    
    float angle = 0.0f;
    default_random_engine generator;
    normal_distribution<float> noise{0.0f, 0.05f};
    
    void generate_sensor_data(float& x, float& y, float& z) {
        angle += 0.1f;
        
        x = sin(angle) * 0.5f + noise(generator);
        y = 9.81f + sin(angle * 1.3f) * 0.3f + noise(generator);
        z = cos(angle * 0.7f) * 0.4f + noise(generator);
        
        x = max(-10.0f, min(10.0f, x));
        y = max(-10.0f, min(20.0f, y));
        z = max(-10.0f, min(10.0f, z));
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
        
        json handshake = {{"role", "accelerometer"}};
        string msg = handshake.dump() + "\n";
        send(sock_fd, msg.c_str(), msg.length(), 0);
        
        return true;
    }
    
    void send_loop() {
        cout << "[" << get_current_time_str() 
                 << "] Начата отправка данных (частота ~50 Гц)" << endl;
        
        auto last_time = chrono::steady_clock::now();
        const auto interval = chrono::milliseconds(20); 
        
        while (running) {
            float x, y, z;
            generate_sensor_data(x, y, z);
            
            json data = {
                {"timestamp", get_timestamp_ms()},
                {"x", round(x * 10000.0f) / 10000.0f},
                {"y", round(y * 10000.0f) / 10000.0f},
                {"z", round(z * 10000.0f) / 10000.0f}
            };
            
            string msg = data.dump() + "\n";
            int sent = send(sock_fd, msg.c_str(), msg.length(), 0);
            
            if (sent < 0) {
                cerr << "[" << get_current_time_str() 
                         << "] Ошибка отправки: " << strerror(errno) << endl;
                break;
            }
            
            cout << "[" << get_current_time_str() 
                     << "] Отправлено: x=" << x << " y=" << y << " z=" << z << endl;
            
            auto now = chrono::steady_clock::now();
            auto elapsed = now - last_time;
            if (elapsed < interval) {
                this_thread::sleep_for(interval - elapsed);
            }
            last_time = chrono::steady_clock::now();
        }
    }
    
    void receive_loop() {
        string buffer;
        char ch;
        
        while (running) {
            int n = recv(sock_fd, &ch, 1, 0);
            if (n <= 0) {
                if (running) {
                    cerr << "[" << get_current_time_str() 
                             << "] Соединение с сервером разорвано" << endl;
                }
                break;
            }
            
            if (ch == '\n') {
                try {
                    json data = json::parse(buffer);
                    if (data.contains("timestamp") && data.contains("module")) {
                        int64_t ts = data["timestamp"];
                        float module = data["module"];
                        
                        cout << "[" << get_current_time_str() 
                                 << "] Получен модуль: " << module << endl;
                        
                        if (log_file.is_open()) {
                            log_file << ts << " " << fixed << setprecision(6) 
                                    << module << endl;
                            log_file.flush();
                        }
                    }
                    buffer.clear();
                } catch (const exception& e) {
                    cerr << "[" << get_current_time_str() 
                             << "] Ошибка парсинга JSON: " << e.what() << endl;
                    buffer.clear();
                }
            } else {
                buffer += ch;
            }
        }
    }
    
public:
    AccelerometerClientA(const string& ip, int port) 
        : server_ip(ip), server_port(port), running(true) {
        
        log_file.open("accel_module.log", ios::out | ios::app);
        if (!log_file.is_open()) {
            cerr << "[" << get_current_time_str() 
                     << "] Не удалось открыть accel_module.log" << endl;
        } else {
            cout << "[" << get_current_time_str() 
                     << "] Лог файл открыт: accel_module.log" << endl;
        }
    }
    
    void run() {
        if (!connect_to_server()) {
            return;
        }
        
        send_thread = thread(&AccelerometerClientA::send_loop, this);
        receive_thread = thread(&AccelerometerClientA::receive_loop, this);
        
        send_thread.join();
        receive_thread.join();
    }
    
    ~AccelerometerClientA() {
        running = false;
        if (sock_fd >= 0) {
            close(sock_fd);
        }
        if (log_file.is_open()) {
            log_file.close();
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
    
    cout << "=== Клиент A (эмулятор акселерометра) ===" << endl;
    cout << "Сервер: " << server_ip << ":" << port << endl;
    
    AccelerometerClientA client(server_ip, port);
    client.run();
    
    return 0;
}