# Teste da Ethernet1

Aplicação isolada para validar a Ethernet1 Xplained Pro com controlador
KSZ8851SNL no conector EXT3 da SAM D21 Xplained Pro.

## Hardware

- Ethernet1 conectada no **EXT3**;
- cabo de rede conectado a um roteador ou switch com DHCP;
- alimentação USB da SAM D21.

## Compilar

Na raiz do projeto:

```bash
west build -d build/servidor-rede \
  -b samd21_xpro \
  servidor/rede \
  -p always
```

O firmware ficará em:

```text
build/servidor-rede/zephyr/zephyr.hex
```

## Resultado esperado

Após gravar o HEX e conectar um cabo de rede a uma rede com DHCP, a serial
deve mostrar:

```text
Teste Ethernet1 KSZ8851SNL no EXT3
Aguardando link e DHCP...
Interface Ethernet ativa
Endereco IPv4 adicionado
IP obtido por DHCP: 192.168.x.x
```

O servidor de DHCP pode entregar outro endereço. O importante é aparecer um
IPv4 diferente de `0.0.0.0`.
