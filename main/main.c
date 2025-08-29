/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

// Add semaphore
#include "freertos/semphr.h"

SemaphoreHandle_t xBinarySemaphore1 = NULL;
SemaphoreHandle_t xBinarySemaphore2 = NULL;
SemaphoreHandle_t xBinarySemaphore3 = NULL;

void vTask1(void *pvParameters) {
    int vez = 1; // alterna entre semáforos
    while (1) 
    {
        if(vez == 1) {
            xSemaphoreGive(xBinarySemaphore1); // Libera o semáforo 1
            vez = 2; // Alterna para o semáforo 2
        } else if (vez == 2) {
            xSemaphoreGive(xBinarySemaphore2); // Libera o semáforo 2
            vez = 3; // Alterna para o semáforo 3
        } else {
            xSemaphoreGive(xBinarySemaphore3); // Libera o semáforo 3
            vez = 1; // Alterna para o semáforo 1
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 1 segundo
    }
}

void vTask2(void *pvParameters) {
    while (1) 
    {
        if(xSemaphoreTake(xBinarySemaphore1, portMAX_DELAY) == pdTRUE) 
        {
            printf("[TAREFA 1] Executou - Matheus Gava Silva\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 1 segundo
    }
}

void vTask3(void *pvParameters) {
    while (1) 
    {
        if(xSemaphoreTake(xBinarySemaphore2, portMAX_DELAY) == pdTRUE) 
        {
            printf("[TAREFA 2] Executou - Matheus Gava Silva\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 1 segundo
    }
}

void vTask4(void *pvParameters) {
    while (1) 
    {
        if(xSemaphoreTake(xBinarySemaphore3, portMAX_DELAY) == pdTRUE) 
        {
            printf("[TAREFA 3] Executou - Matheus Gava Silva\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de 1 segundo
    }
}


void app_main(void) {
    xBinarySemaphore1 = xSemaphoreCreateBinary();
    xBinarySemaphore2 = xSemaphoreCreateBinary();
    xBinarySemaphore3 = xSemaphoreCreateBinary();

    if (xBinarySemaphore1 == NULL && xBinarySemaphore2 == NULL && xBinarySemaphore3 == NULL) {
        printf("Falha ao criar semáforo binário\n");
        return;
    }


    xTaskCreate(vTask1, // Função da task
                "Task1", // Nome da task
                2048, // Stack size em bytes
                NULL, // Parâmetros da task
                5, // Prioridade da task
                NULL /* Handle da task (opcional) */ );

    xTaskCreate(vTask2, // Função da task
                "Task2", // Nome da task
                2048, // Stack size em bytes
                NULL, // Parâmetros da task
                5, // Prioridade da task
                NULL /* Handle da task (opcional) */ );

    xTaskCreate(vTask3, // Função da task
                "Task3", // Nome da task
                2048, // Stack size em bytes
                NULL, // Parâmetros da task
                5, // Prioridade da task
                NULL /* Handle da task (opcional) */ );

    xTaskCreate(vTask4, // Função da task
                "Task4", // Nome da task
                2048, // Stack size em bytes
                NULL, // Parâmetros da task
                5, // Prioridade da task
                NULL /* Handle da task (opcional) */ );
}