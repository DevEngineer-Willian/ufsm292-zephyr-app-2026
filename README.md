# Zephyr Example Application

<a href="https://github.com/zephyrproject-rtos/example-application/actions/workflows/build.yml?query=branch%3Amain">
  <img src="https://github.com/zephyrproject-rtos/example-application/actions/workflows/build.yml/badge.svg?event=push">
</a>
<a href="https://github.com/zephyrproject-rtos/example-application/actions/workflows/docs.yml?query=branch%3Amain">
  <img src="https://github.com/zephyrproject-rtos/example-application/actions/workflows/docs.yml/badge.svg?event=push">
</a>
<a href="https://zephyrproject-rtos.github.io/example-application">
  <img alt="Documentation" src="https://img.shields.io/badge/documentation-3D578C?logo=sphinx&logoColor=white">
</a>
<a href="https://zephyrproject-rtos.github.io/example-application/doxygen">
  <img alt="API Documentation" src="https://img.shields.io/badge/API-documentation-3D578C?logo=c&logoColor=white">
</a>

This repository contains a Zephyr example application. The main purpose of this
repository is to serve as a reference on how to structure Zephyr-based
applications. Some of the features demonstrated in this example are:

- Basic [Zephyr application][app_dev] skeleton
- [Zephyr workspace applications][workspace_app]
- [Zephyr modules][modules]
- [West T2 topology][west_t2]
- [Custom boards][board_porting]
- Custom [devicetree bindings][bindings]
- Out-of-tree [drivers][drivers]
- Out-of-tree libraries
- Example CI configuration (using GitHub Actions)
- Custom [west extension][west_ext]
- Custom [Zephyr runner][runner_ext]
- Doxygen and Sphinx documentation boilerplate

This repository is versioned together with the [Zephyr main tree][zephyr]. This
means that every time that Zephyr is tagged, this repository is tagged as well
with the same version number, and the [manifest](west.yml) entry for `zephyr`
will point to the corresponding Zephyr tag. For example, the `example-application`
v2.6.0 will point to Zephyr v2.6.0. Note that the `main` branch always
points to the development branch of Zephyr, also `main`.

[app_dev]: https://docs.zephyrproject.org/latest/develop/application/index.html
[workspace_app]: https://docs.zephyrproject.org/latest/develop/application/index.html#zephyr-workspace-app
[modules]: https://docs.zephyrproject.org/latest/develop/modules.html
[west_t2]: https://docs.zephyrproject.org/latest/develop/west/workspaces.html#west-t2
[board_porting]: https://docs.zephyrproject.org/latest/guides/porting/board_porting.html
[bindings]: https://docs.zephyrproject.org/latest/guides/dts/bindings.html
[drivers]: https://docs.zephyrproject.org/latest/reference/drivers/index.html
[zephyr]: https://github.com/zephyrproject-rtos/zephyr
[west_ext]: https://docs.zephyrproject.org/latest/develop/west/extensions.html
[runner_ext]: https://docs.zephyrproject.org/latest/develop/modules.html#external-runners

## Getting Started

Before getting started, make sure you have a proper Zephyr development
environment. Follow the official
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

### Initialization

The first step is to initialize the workspace folder (``my-workspace``) where
the ``example-application`` and all Zephyr modules will be cloned. Run the following
command:

```shell
# initialize my-workspace for the example-application (main branch)
west init -m https://github.com/zephyrproject-rtos/example-application --mr main my-workspace
# update Zephyr modules
cd my-workspace
west update
```

### Building and running

To build the application, run the following command:

```shell
cd example-application
west build -b $BOARD app
```

where `$BOARD` is the target board.

Zephyr sample boards may be used if an appropriate overlay is provided
(see `app/boards`).

A sample debug configuration is also provided. To apply it, run the following
command:

```shell
west build -b $BOARD app -- -DEXTRA_CONF_FILE=debug.conf
```

Once you have built the application, run the following command to flash it:

```shell
west flash
```

#### Gateway (SAM R21 Xplained Pro)

To build the gateway application for the SAM R21 Xplained Pro board:

```shell
west build -b samr21_xpro gateway
```

### Testing

To execute Twister integration tests, run the following command:

```shell
west twister -T tests --integration
```

### Documentation

A minimal documentation setup is provided for Doxygen and Sphinx. To build the
documentation first change to the ``doc`` folder:

```shell
cd doc
```

Before continuing, check if you have Doxygen installed. It is recommended to
use the same Doxygen version used in [CI](.github/workflows/docs.yml). To
install Sphinx, make sure you have a Python installation in place and run:

```shell
pip install -r requirements.txt
```

API documentation (Doxygen) can be built using the following command:

```shell
doxygen
```

The output will be stored in the ``_build_doxygen`` folder. Similarly, the
Sphinx documentation (HTML) can be built using the following command:

```shell
make html
```

The output will be stored in the ``_build_sphinx`` folder. You may check for
other output formats other than HTML by running ``make help``.



---------------------------------------------------------------------------------------
# UFSM292 - Nó Sensor (Semanas 1-2)

Este repositório contém o código-fonte do Nó Sensor, desenvolvido no Zephyr RTOS para a placa ATSAMR21-XPRO.

## Funcionalidades Implementadas até agora

- **Hardware e DeviceTree:** Configuração do barramento I2C (`sercom1`) e mapeamento do sensor inercial BNO055 (`0x28`).
- **Leitura I2C (Raw):** Implementação manual da leitura do BNO055 utilizando a API nativa do Zephyr, incluindo verificação de Chip ID e leitura contínua (burst read) dos eixos X, Y e Z da aceleração.
- **Empacotamento de Dados:** Criação de uma estrutura C (`struct`) com a diretiva `__attribute__((packed))`, garantindo um pacote de dados binário de exatamente 18 bytes (Node ID, Sequência, Luz, Temp, Aceleração X/Y/Z, Uptime e Flags).
- **Rádio 802.15.4:** Ativação do subsistema de rede do Zephyr e rádio AT86RF233. Utilização de Raw Sockets (`AF_PACKET`) para injeção direta dos pacotes na rede sem fios (sem pilha IP).

## Como compilar e executar
# 1. Ativar o ambiente virtual
C:\caminho\para\zephyrproject.venv\Scripts\Activate.ps1

# 2. Compilar o projeto
west build -p always -b samr21_xpro app

# 3. Gravar na placa
west flash


## Servidor IoT (Ethernet KSZ8851SNL + Cartão SD)

Implementação do servidor HTTP e coletor de dados na plataforma **ATSAMD21J18A** (`samd21_xpro`) com interface Ethernet via **Microchip KSZ8851SNL** (SPI / SERCOM5) e armazenamento persistente em cartão microSD (`/SD:/LEITURAS.CSV`).

### Pré-requisitos de Kernel (Driver Ethernet)

O controlador **KSZ8851SNL** sobre SPI exige o driver correspondente habilitado na árvore do Zephyr RTOS (`v4.5.0-rc1` ou superior):

* **Branch do Zephyr:** `ksz8851-driver` (contendo `drivers/ethernet/eth_ksz8851snl.c` e o binding `microchip,ksz8851snl.yaml`)
* **Kconfig:** `CONFIG_ETH_KSZ8851SNL=y`
* **Ajuste de RAM/Stack (SAMD21 - 32 KB SRAM):** `CONFIG_NET_TCP_WORKQ_STACK_SIZE=2048`

### Compilação e Gravação

Com o ambiente virtual do Zephyr ativo, compile e grave o firmware a partir da raiz do projeto:

```bash
west build -b samd21_xpro servidor -p always
west flash

### Configuração de Rede

Por padrão, o servidor utiliza endereçamento IPv4 estático (CONFIG_NET_DHCPV4=n em servidor/prj.conf) para operação direta em bancada:

- IP do Servidor (Placa): 192.168.0.200 (Máscara 255.255.255.0)
- Porta da API HTTP: 5501 (Endpoints /health e /log?node=<id>&n=<qtd>)
- Gateway Alvo (Coletor): 192.168.0.100:5500 (GET /sensors a cada 5000 ms)

Preparando o Host (Conexão Direta RJ45):

  sudo ip link set enp0s25 up
  sudo ip addr replace 192.168.0.100/24 dev enp0s25
  sudo ip route replace 192.168.0.200/32 dev enp0s25

Validação Rápida:

  curl -i "http://192.168.0.200:5501/health"
  curl -i "http://192.168.0.200:5501/log?node=1&n=5"
