# NoFTP

Для сборки клиента:
g++ -std=c++11 -fsanitize=address client.cpp -o сlient

Для сборки сервера:
g++ -std=c++11 -fsanitize=address server.cpp -o server

Для запуска сервера:
./server
Для запуска сервера в бэкграунде:
./server &

Для запуска клиента:
./client

