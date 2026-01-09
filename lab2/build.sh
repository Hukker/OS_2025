#!/bin/bash

set -e

echo "Очистка предыдущей сборки..."
rm -rf build
rm -f host_* client_*

echo "Создание директории build..."
mkdir -p build
cd build

echo "Генерация Makefile..."
cmake ..

echo "Компиляция..."
make -j$(nproc)

echo "Копирование исполняемых файлов..."
cp host_* ../
cp client_* ../

echo "Очистка промежуточных файлов..."
cd ..
rm -rf build

echo "Сборка завершена успешно!"
echo ""
echo "Доступные программы:"
echo "  Хосты:"
for host in host_*; do
    echo "    ./$host <тип> <имя_пользователя>"
done
echo ""
echo "  Клиенты:"
for client in client_*; do
    echo "    ./$client <тип> <host_pid> <client_id> <имя_пользователя>"
done