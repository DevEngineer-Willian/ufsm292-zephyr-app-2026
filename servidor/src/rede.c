#include "rede.h"

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/sys/printk.h>

static struct net_mgmt_event_callback net_cb;
static K_SEM_DEFINE(ip_pronto, 0, 1);

static bool mostrar_ipv4(struct net_if *iface)
{
	struct net_in_addr *addr;
	char texto[NET_IPV4_ADDR_LEN];

	addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
	if (addr == NULL) {
		return false;
	}

	net_addr_ntop(AF_INET, addr, texto, sizeof(texto));
	printk("Rede: IP %s\r\n", texto);
	return true;
}

static void net_event_handler(struct net_mgmt_event_callback *cb,
			      uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (event == NET_EVENT_IPV4_ADDR_ADD) {
		k_sem_give(&ip_pronto);
	}
}

void rede_iniciar(void)
{
	net_mgmt_init_event_callback(&net_cb, net_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&net_cb);
}

int rede_aguardar_ip(k_timeout_t timeout)
{
	struct net_if *iface = net_if_get_default();

	/* O DHCP pode ter terminado antes de o callback ser registrado */
	if (mostrar_ipv4(iface)) {
		return 0;
	}

	if (k_sem_take(&ip_pronto, timeout) < 0) {
		return -EAGAIN;
	}

	mostrar_ipv4(iface);
	return 0;
}
