#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <ff.h>
#include <stdio.h>

#define PONTO_MONTAGEM "/SD:"
#define ARQUIVO PONTO_MONTAGEM "/LEITURAS.CSV"
#define CABECALHO "recebido_ms,node_id,light,temp_c,accel_x,accel_y,accel_z,last_seen_ms\n"
#define N_LINHAS 10

/* Mesmo formato de um item do GET /sensors do gateway */
struct leitura {
	uint8_t node_id;
	uint16_t light;
	float temp_c;
	float accel[3];
	uint32_t last_seen_ms;
};

static FATFS fat_fs;
static struct fs_mount_t montagem = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = PONTO_MONTAGEM,
	.flags = FS_MOUNT_FLAG_NO_FORMAT,
};

static int escrever_cabecalho_se_vazio(struct fs_file_t *arq)
{
	struct fs_dirent info;
	int ret = fs_stat(ARQUIVO, &info);

	if (ret < 0) {
		return ret;
	}
	if (info.size > 0) {
		printk("Arquivo ja existe (%u bytes), acrescentando linhas\r\n",
		       (uint32_t)info.size);
		return 0;
	}

	ret = fs_write(arq, CABECALHO, sizeof(CABECALHO) - 1);
	return ret < 0 ? ret : 0;
}

static int gravar_leitura(struct fs_file_t *arq, const struct leitura *l)
{
	char linha[96];
	int n;
	ssize_t escrito;

	n = snprintf(linha, sizeof(linha), "%u,%d,%d,%.2f,%.3f,%.3f,%.3f,%u\n",
		     (uint32_t)k_uptime_get(), l->node_id, l->light,
		     (double)l->temp_c, (double)l->accel[0],
		     (double)l->accel[1], (double)l->accel[2],
		     l->last_seen_ms);

	escrito = fs_write(arq, linha, n);
	if (escrito != n) {
		return escrito < 0 ? (int)escrito : -EIO;
	}

	/* Garante que a linha chegou ao cartão mesmo se a placa reiniciar */
	return fs_sync(arq);
}

static void mostrar_arquivo(void)
{
	struct fs_file_t arq;
	char buf[64];
	ssize_t lido;

	fs_file_t_init(&arq);
	if (fs_open(&arq, ARQUIVO, FS_O_READ) < 0) {
		printk("Falha ao reabrir %s para leitura\r\n", ARQUIVO);
		return;
	}

	printk("\r\n--- conteudo de %s ---\r\n", ARQUIVO);
	while ((lido = fs_read(&arq, buf, sizeof(buf))) > 0) {
		printk("%.*s", (int)lido, buf);
	}
	printk("--- fim ---\r\n");

	fs_close(&arq);
}

int main(void)
{
	struct fs_file_t arq;
	int ret;

	printk("\r\nTeste microSD (IO1 no EXT1)\r\n");

	ret = fs_mount(&montagem);
	if (ret < 0) {
		printk("Falha ao montar o SD (%d)\r\n", ret);
		printk("Confira: cartao inserido, IO1 no EXT1, cartao em FAT32\r\n");
		return 0;
	}
	printk("SD montado em %s\r\n", PONTO_MONTAGEM);

	fs_file_t_init(&arq);
	ret = fs_open(&arq, ARQUIVO, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
	if (ret < 0) {
		printk("Falha ao abrir %s (%d)\r\n", ARQUIVO, ret);
		return 0;
	}

	ret = escrever_cabecalho_se_vazio(&arq);
	if (ret < 0) {
		printk("Falha ao escrever o cabecalho (%d)\r\n", ret);
		fs_close(&arq);
		return 0;
	}

	for (int i = 0; i < N_LINHAS; i++) {
		/* Dados fictícios: 3 nós alternando */
		struct leitura l = {
			.node_id = 1 + (i % 3),
			.light = 300 + 7 * i,
			.temp_c = 24.5f + 0.1f * i,
			.accel = { 0.01f, 0.02f, 9.8f },
			.last_seen_ms = (uint32_t)k_uptime_get() - 100,
		};

		ret = gravar_leitura(&arq, &l);
		if (ret < 0) {
			printk("Falha ao gravar a linha %d (%d)\r\n", i + 1, ret);
			break;
		}
		printk("Linha %d/%d gravada (no %d)\r\n", i + 1, N_LINHAS,
		       l.node_id);
		k_sleep(K_SECONDS(2));
	}

	fs_close(&arq);
	mostrar_arquivo();

	fs_unmount(&montagem);
	printk("SD desmontado, pode remover o cartao\r\n");

	return 0;
}
