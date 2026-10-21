# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ufsm292_zephyr_app_2026)

# Fontes principais da aplicação (ajuste o caminho se seu main.c estiver em outro diretório)
target_sources(app PRIVATE src/main.c)

# Inclusões e diretórios de chamadas de sistema
zephyr_syscall_include_directories(include)
zephyr_include_directories(include)

# Subdiretórios do projeto
add_subdirectory(drivers)
add_subdirectory(lib)