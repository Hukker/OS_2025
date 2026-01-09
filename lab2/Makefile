.PHONY: all clean build

all: build
	@echo "Сборка завершена"

build:
	./build.sh

clean:
	rm -rf build/ CMakeFiles/ CMakeCache.txt cmake_install.cmake
	rm -f host_* client_*
	rm -f /tmp/chat_* /dev/mqueue/chat_*

run_host_mq:
	./build/host_mq mq HostUser

run_client_mq:
	./build/client_mq mq $$(pidof host_mq) 1 Client1

run_host_fifo:
	./build/host_fifo fifo HostUser

run_client_fifo:
	./build/client_fifo fifo $$(pidof host_fifo) 1 Client1

run_host_sock:
	./build/host_sock sock HostUser

run_client_sock:
	./build/client_sock sock $$(pidof host_sock) 1 Client1