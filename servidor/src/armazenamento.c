#include "armazenamento.h"

#include "relogio.h"

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <ff.h>
#include <errno.h>
#include <stdio.h>

#define PONTO_MONTAGEM "/SD:"
#define ARQUIVO PONTO_MONTAGEM "/LEITURAS.CSV"
/* data_hora fica vazia se o NTP ainda não respondeu; recebido_ms é o uptime */
#define CABECALHO "data_hora,recebido_ms,node_id,light,temp_c,accel_x,accel_y,accel_z,last_seen_ms\n"

static FATFS fat_fs;
static struct fs_mount_t montagem = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = PONTO_MONTAGEM,
	.flags = FS_MOUNT_FLAG_NO_FORMAT,
};

/* O coletor grava e o HTTP vai ler o histórico: um de cada vez no cartão */
static K_MUTEX_DEFINE(trava_sd);
static bool montado;

static int escrever(const char *texto, size_t tam)
{
	struct fs_file_t arq;
	ssize_t escrito;
	int ret;

	/* Abre e fecha a cada linha: se a placa reiniciar, nada fica pendente */
	fs_file_t_init(&arq);
	ret = fs_open(&arq, ARQUIVO, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
	if (ret < 0) {
		return ret;
	}

	escrito = fs_write(&arq, texto, tam);
	ret = fs_close(&arq);

	if (escrito != (ssize_t)tam) {
		return escrito < 0 ? (int)escrito : -EIO;
	}
	return ret;
}

int armazenamento_iniciar(void)
{
	struct fs_dirent info;
	int ret;

	ret = fs_mount(&montagem);
	if (ret < 0) {
		printk("SD: falha ao montar (%d)\r\n", ret);
		return ret;
	}
	montado = true;

	ret = fs_stat(ARQUIVO, &info);
	if (ret == 0 && info.size > 0) {
		printk("SD: %s com %u bytes, acrescentando\r\n", ARQUIVO,
		       (uint32_t)info.size);
		return 0;
	}

	ret = escrever(CABECALHO, sizeof(CABECALHO) - 1);
	if (ret < 0) {
		printk("SD: falha ao criar %s (%d)\r\n", ARQUIVO, ret);
		return ret;
	}

	printk("SD: %s criado\r\n", ARQUIVO);
	return 0;
}

bool armazenamento_ok(void)
{
	return montado;
}

void armazenamento_finalizar(void)
{
	/* Espera qualquer gravação em andamento e não solta mais a trava */
	k_mutex_lock(&trava_sd, K_FOREVER);
	if (montado) {
		fs_unmount(&montagem);
		montado = false;
	}
}

int armazenamento_gravar(const struct leitura *l)
{
	char linha[128];
	char data_hora[20];
	int n;
	int ret;

	if (!montado) {
		return -ENODEV;
	}

	relogio_agora(data_hora, sizeof(data_hora));
	n = snprintf(linha, sizeof(linha), "%s,%u,%u,%u," TEMP_FMT ",%d,%d,%d,%u\n",
		     data_hora, (uint32_t)k_uptime_get(), l->node_id, l->light,
		     TEMP_ARGS(l->temp_c_x100), l->accel[0], l->accel[1],
		     l->accel[2], l->last_seen_ms);

	k_mutex_lock(&trava_sd, K_FOREVER);
	ret = escrever(linha, n);
	k_mutex_unlock(&trava_sd);

	return ret;
}
