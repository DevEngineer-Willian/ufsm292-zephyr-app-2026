cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(servidor LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/rede.c
	src/armazenamento.c
	src/relogio.c
	src/api_http.c
)
