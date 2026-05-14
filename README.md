Вариант развертывания клиентских узлов: 2 (Linux).

Уровень сложности 1 (Базовый) — TCP-сокеты и JSON.

Транспорт: обычные TCP-сокеты (IPv4).

Сериализация: JSON.

# Описание архитектуры (Уровень 1)
```
text
[Client A]  --(TCP: JSON)-->  [Server]  --(TCP: JSON)-->  [Client B]
   ^                            |  ^                          |
   |                            |  |                          v
   +-------(TCP: JSON)----------+  +-----(TCP: JSON)-----[вычисление]
```
Client A (эмулятор акселерометра) генерирует данные с частотой ~50 Гц (синусоидальная эмуляция).

Сервер принимает подключения от A и B, хранит последнее значение от A, отбрасывает дубликаты (сравнение x,y,z с 4 знаками), пересылает валидные данные в B.

Client B получает данные, вычисляет sqrt(x²+y²+z²) и отправляет результат обратно на сервер.

Сервер пересылает результат клиенту A.

Client A записывает модуль ускорения с меткой времени в accel_module.log.

Все сообщения — JSON с \n в конце.

Сборка и запуск (Linux Ubuntu 22.04)
1. Установка зависимостей
```bash
sudo apt update
sudo apt install -y build-essential cmake nlohmann-json3-dev
```

2. Клонирование репозитория
```bash
git clone https://github.com/NoF3XL/TestYadro3.git
cd TestYadro3
```

3. Сборка всех компонентов bash 

## Сервер
```
cd server && mkdir build && cd build
cmake .. && make
cd ../..
```
## Client A
```
cd client_a && mkdir build && cd build
cmake .. && make
cd ../..
```
## Client B
```
cd client_b && mkdir build && cd build
cmake .. && make
cd ../..
```
Или используйте скрипт сборки (прилагается в репозитории).

4. Запуск (три терминала)

Терминал 1 — Сервер
```bash
cd server/build
./server 8080
```
Терминал 2 — Client B
```bash
cd client_b/build
./client_b 127.0.0.1 8080
```
Терминал 3 — Client A
```bash
cd client_a/build
./client_a 127.0.0.1 8080
```
После запуска Client A начнёт эмуляцию акселерометра и вывод в консоль, а также запишет модули в client_a/build/accel_module.log.
