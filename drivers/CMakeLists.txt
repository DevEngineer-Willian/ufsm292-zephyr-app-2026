# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

zephyr_syscall_header(${CMAKE_CURRENT_LIST_DIR}/../include/app/drivers/blink.h)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/../include)

# Out-of-tree drivers for custom classes
add_subdirectory_ifdef(CONFIG_BLINK blink)

# Out-of-tree drivers for existing driver classes
add_subdirectory_ifdef(CONFIG_ETH_KSZ8851SNL ethernet)
add_subdirectory_ifdef(CONFIG_SENSOR sensor)
