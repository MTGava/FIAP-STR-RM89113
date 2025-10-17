#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

/* Prioridades */
#define GEN_TASK_PRIO      6 
#define RX_TASK_PRIO       5
#define SUP_TASK_PRIO      4

#define GEN_STACK_WORDS    4096
#define RX_STACK_WORDS     4096
#define SUP_STACK_WORDS    4096

#define QUEUE_LEN          10
#define QUEUE_ITEM_SIZE    sizeof(int)

/* TIMEOUTS */
#define GEN_PERIOD_MS            150
#define RX_TIMEOUT_MS            1000
#define SUP_PERIOD_MS            1500
#define STALL_TICKS(ms)          pdMS_TO_TICKS(ms)
#define WDT_TIMEOUT_SECONDS      5

/* Handles globais */
static QueueHandle_t g_queue = NULL;
static TaskHandle_t g_task_gen = NULL;
static TaskHandle_t g_task_rx  = NULL;
static TaskHandle_t g_task_sup = NULL;

/* Heartbeats para monitoramento */
static volatile TickType_t g_hb_gen = 0;
static volatile TickType_t g_hb_rx  = 0;
static volatile TickType_t g_hb_sup = 0;

static volatile bool g_flag_gen_ok = false;
static volatile bool g_flag_rx_ok  = false;

// Módulo 1 – Geração de Dados
static void task_geradora(void *pv) {
    esp_task_wdt_add(NULL);

    int value = 0;
    for (;;) {
        if (xQueueSend(g_queue, &value, 0) == pdTRUE) {
            g_hb_gen = xTaskGetTickCount();
            g_flag_gen_ok = true;
            printf("{Matheus Gava Silva - RM:89113} [GERADOR] Valor: %d enfileirado.\n", value);
            value++;
        } else {
            printf("{Matheus Gava Silva - RM:89113} [GERADOR] Valor: %d descartado (fila cheia).\n", value);
            value++;
        }

        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        if (watermark < 100) {
            printf("{Matheus Gava Silva - RM:89113} [GERADOR] Atenção: pouca pilha restante (%u words).\n", (unsigned)watermark);
        }

        esp_task_wdt_reset();
        vTaskDelay(STALL_TICKS(GEN_PERIOD_MS));
    }
}

// Módulo 2 – Recepção e Transmissão
static void task_receptora(void *pv) {
    esp_task_wdt_add(NULL);

    int timeouts = 0;

    for (;;) {
        int rx_val = 0;
        if (xQueueReceive(g_queue, &rx_val, STALL_TICKS(RX_TIMEOUT_MS)) == pdTRUE) {
            timeouts = 0;
            g_hb_rx = xTaskGetTickCount();
            g_flag_rx_ok = true;

            int *tmp = (int*) malloc(sizeof(int));
            if (!tmp) {
                printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] ERRO CRÍTICO: malloc falhou – sem memória.\n");
                g_flag_rx_ok = false;
                break;
            }
            *tmp = rx_val;

            printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Transmitindo valor: %d\n", *tmp);

            free(tmp);

        } else {
            timeouts++;
            printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Timeout de %d ms na fila (contagem=%d).\n", RX_TIMEOUT_MS, timeouts);

            if (timeouts == 2) {
                printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Aviso.\n");
            } else if (timeouts == 3) {
                printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Recuperação.\n");
                xQueueReset(g_queue);
            } else if (timeouts >= 4) {
                printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Encerramento.\n");
                g_flag_rx_ok = false;
                break;
            }
        }

        size_t free_heap = xPortGetFreeHeapSize();
        size_t min_heap  = xPortGetMinimumEverFreeHeapSize();
        if (free_heap < (20 * 1024)) {
            printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Pouca memória livre: %u bytes (mínimo histórico %u).\n",
                   (unsigned)free_heap, (unsigned)min_heap);
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    printf("{Matheus Gava Silva - RM:89113} [RECEBEDOR_TRANSMISSOR] Tarefa será finalizada para permitir recriação.\n");
    vTaskDelete(NULL);
}

// Módulo 3 – Supervisão
static void task_supervisor(void *pv) {
    esp_task_wdt_add(NULL);

    int rx_restarts = 0;

    for (;;) {
        vTaskDelay(STALL_TICKS(SUP_PERIOD_MS));
        TickType_t now = xTaskGetTickCount();
        g_hb_sup = now;

        printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Status – GERADOR:%s (hb=%u) | RECEBEDOR_TRANSMISSOR:%s (hb=%u)\n",
               g_flag_gen_ok ? "OK" : "ERRO",
               (unsigned)g_hb_gen,
               g_flag_rx_ok  ? "OK" : "ERRO",
               (unsigned)g_hb_rx);

        if ((now - g_hb_gen) > STALL_TICKS(3 * SUP_PERIOD_MS)) {
            printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Detetado GERADOR inativo – reiniciando tarefa.\n");
            if (g_task_gen) {
                vTaskDelete(g_task_gen);
                g_task_gen = NULL;
            }
            xTaskCreatePinnedToCore(task_geradora, "task_geradora",
                                    GEN_STACK_WORDS, NULL, GEN_TASK_PRIO, &g_task_gen, 1);
            g_hb_gen = xTaskGetTickCount();
            g_flag_gen_ok = false;
        }

        if (g_task_rx == NULL || (now - g_hb_rx) > STALL_TICKS(5 * SUP_PERIOD_MS)) {
            printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Detetada RECEBEDOR_TRANSMISSOR inativa – recriando tarefa.\n");
            if (g_task_rx) {
                vTaskDelete(g_task_rx);
                g_task_rx = NULL;
            }
            xTaskCreatePinnedToCore(task_receptora, "task_receptora",
                                    RX_STACK_WORDS, NULL, RX_TASK_PRIO, &g_task_rx, 1);
            rx_restarts++;
            g_hb_rx = xTaskGetTickCount();
            g_flag_rx_ok = false;

            if (rx_restarts >= 3) {
                size_t free_heap = xPortGetFreeHeapSize();
                if (free_heap < (16 * 1024)) {
                    printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Memória crítica após várias recriações (%u bytes). Reiniciando dispositivo...\n",
                           (unsigned)free_heap);
                    esp_restart();
                }
            }
        }

        size_t free_heap = xPortGetFreeHeapSize();
        size_t min_heap  = xPortGetMinimumEverFreeHeapSize();
        printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Heap livre=%u bytes (mínimo histórico %u).\n",
               (unsigned)free_heap, (unsigned)min_heap);
        if (min_heap < (8 * 1024)) {
            printf("{Matheus Gava Silva - RM:89113} [SUPERVISOR] Heap mínimo crítico – reiniciando dispositivo...\n");
            esp_restart();
        }

        esp_task_wdt_reset();
    }
}

void app_main(void) {
    printf("{Matheus Gava Silva - RM:89113} [BOOT] Iniciando sistema...\n");
    // esp_task_wdt_deinit();
    // esp_task_wdt_config_t wdt_cfg = {
    //     .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
    //     .trigger_panic = true,
    // };
    // esp_task_wdt_init(&wdt_cfg);

    g_queue = xQueueCreate(QUEUE_LEN, QUEUE_ITEM_SIZE);
    if (!g_queue) {
        printf("{Matheus Gava Silva - RM:89113} [BOOT] Falha ao criar fila. Reiniciando sistema...\n");
        esp_restart();
    }

    xTaskCreatePinnedToCore(task_geradora,  "task_geradora",   GEN_STACK_WORDS, NULL, GEN_TASK_PRIO, &g_task_gen, 1);
    xTaskCreatePinnedToCore(task_receptora, "task_receptora",  RX_STACK_WORDS,  NULL, RX_TASK_PRIO,  &g_task_rx,  1);
    xTaskCreatePinnedToCore(task_supervisor,"task_supervisor", SUP_STACK_WORDS, NULL, SUP_TASK_PRIO, &g_task_sup, 1);

    printf("{Matheus Gava Silva - RM:89113} [BOOT] Sistema iniciado com sucesso.\n");
}
