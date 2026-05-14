#!/bin/bash

echo "=== Сборка распределенной системы обработки данных акселерометра ==="

mkdir -p build
cd build

echo "Сборка сервера..."
mkdir -p server && cd server
cmake ../../server && make
cd ..

echo "Сборка клиента A..."
mkdir -p client_a && cd client_a
cmake ../../client_a && make
cd ..

echo "Сборка клиента B..."
mkdir -p client_b && cd client_b
cmake ../../client_b && make
cd ../..

echo "=== Сборка завершена ==="
echo "Запуск:"
echo "1. Сервер: ./build/server/server <порт>"
echo "2. Клиент B: ./build/client_b/client_b <IP_сервера> <порт>"
echo "3. Клиент A: ./build/client_a/client_a <IP_сервера> <порт>"