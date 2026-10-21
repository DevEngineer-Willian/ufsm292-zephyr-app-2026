#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/sys/printk.h>

static struct net_mgmt_event_callback net_cb;

static void print_ipv4(struct net_if *iface)
{
	struct net_in_addr *addr;
	char text[NET_IPV4_ADDR_LEN];

	addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
	if (addr == NULL) {
		printk("IPv4 ainda nao esta disponivel\r\n");
		return;
	}

	net_addr_ntop(AF_INET, addr, text, sizeof(text));
	printk("IP obtido por DHCP: %s\r\n", text);
}

static void net_event_handler(struct net_mgmt_event_callback *cb,
			      uint64_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (event == NET_EVENT_IF_UP) {
		printk("Interface Ethernet ativa\r\n");
		print_ipv4(iface);
	} else if (event == NET_EVENT_IPV4_ADDR_ADD) {
		printk("Endereco IPv4 adicionado\r\n");
		print_ipv4(iface);
	}
}

int main(void)
{
	struct net_if *iface = net_if_get_default();

	printk("\r\nTeste Ethernet1 KSZ8851SNL no EXT3\r\n");
	printk("Aguardando link e DHCP...\r\n");

	net_mgmt_init_event_callback(&net_cb, net_event_handler,
				     NET_EVENT_IF_UP | NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&net_cb);

	while (true) {
		if (iface != NULL && net_if_is_up(iface)) {
			print_ipv4(iface);
		}
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
