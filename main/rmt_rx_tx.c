#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "receive_command.h"
#include "send_command.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "uart_events";
static const char *TAG_NVS = "nvs_events";

#define RX_RMT_CHANNEL RMT_CHANNEL_0
#define TX_RMT_CHANNEL RMT_CHANNEL_4
#define RX_GPIO 21
#define TX_GPIO 15

#define EX_UART_NUM UART_NUM_1
#define UART_GPIO GPIO_NUM_3

#define REC 82
#define SEND 87

#define STORAGE_NAMESPACE "aircon_commands"
#define COMMANDS_KEY_NAME "stored_commands"

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;

struct command {
	uint8_t cmd;
	char brand[15];
	char model[15];
	char func [15];
}__attribute__((packed));

commands *recorded_commands;

size_t recorded_size = 0;

esp_err_t get_commands() {

	nvs_handle my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// obtain required memory space to store blob being read from NVS
	err = nvs_get_blob(my_handle, COMMANDS_KEY_NAME, NULL, &recorded_size);
	if (err != ESP_OK) return err;

    if (recorded_size > 0) {
    	recorded_commands = malloc(recorded_size);
        err = nvs_get_blob(my_handle, COMMANDS_KEY_NAME, recorded_commands, &recorded_size);
        ESP_LOGI(TAG_NVS, "Comandos gravados: ");
        ESP_LOG_BUFFER_HEXDUMP(TAG_NVS, recorded_commands, recorded_size, 3);
        if (err != ESP_OK) {
    		free(recorded_commands);
    		nvs_close(my_handle);
    		return err;
        }
    }

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t save_command(commands command) {

    nvs_handle my_handle;
    esp_err_t err;
    int i;
    bool repeated = false;
    int numbers_of_commands;
    size_t new_size;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

	// obtain required memory space to store blob being read from NVS
	err = nvs_get_blob(my_handle, COMMANDS_KEY_NAME, NULL, &recorded_size);

	numbers_of_commands = recorded_size/sizeof(commands);

    ESP_LOGI(TAG_NVS, "NUMBER OF COMMANDS, SIZE: %d, %d", numbers_of_commands, recorded_size);

    ESP_LOGI(TAG_NVS, "RECORDED_COMMANDS: ");
    ESP_LOG_BUFFER_HEXDUMP(TAG_NVS, recorded_commands, recorded_size, 1);
    ESP_LOGI(TAG_NVS, "COMMANDS: ");
    ESP_LOG_BUFFER_HEXDUMP(TAG_NVS, &command, sizeof(commands), 1);

	ESP_LOGI(TAG_NVS, "BRAND TO SAVE: %s", command.brand);
	ESP_LOGI(TAG_NVS, "MODEL TO SAVE: %s", command.model);
	ESP_LOGI(TAG_NVS, "FUNC TO SAVE: %s", command.func);

    if (numbers_of_commands > 0) {
        for (i = 0; i < numbers_of_commands; i++){
			ESP_LOGI(TAG_NVS, "BRAND[%d]: %s", i, recorded_commands[i].brand);
			ESP_LOGI(TAG_NVS, "MODEL[%d]: %s", i, recorded_commands[i].model);
			ESP_LOGI(TAG_NVS, "FUNC[%d]: %s", i, recorded_commands[i].func);
			if(strcmp(recorded_commands[i].brand,command.brand) == 0
			   && strcmp(recorded_commands[i].model, command.model) == 0
			   && strcmp(recorded_commands[i].func, command.func) == 0) {
				memcpy(recorded_commands + i , &command, sizeof(commands));
				repeated = true;
				break;
			}
		}
        if(!repeated) {
        	new_size = recorded_size + sizeof(commands);
        	recorded_commands = (commands *)realloc((commands *)recorded_commands, new_size);
        	ESP_LOGI(TAG_NVS, "SIZE: %d", recorded_size);
        	ESP_LOGI(TAG_NVS, "NEW SIZE: %d", new_size);
        	memcpy(recorded_commands + numbers_of_commands, &command, sizeof(commands));
        	recorded_size = new_size;
        }
    } else {
    	recorded_size = sizeof(commands);
		recorded_commands = malloc(recorded_size);
		memcpy(recorded_commands, &command, recorded_size);
    }

    ESP_LOGI(TAG_NVS, "NUMBER OF COMMANDS, SIZE (AFTER): %d, %d", recorded_size/sizeof(commands), recorded_size);
    ESP_LOG_BUFFER_HEXDUMP(TAG_NVS, recorded_commands, recorded_size, 1);

    // Write
    err = nvs_set_blob(my_handle, COMMANDS_KEY_NAME, recorded_commands, recorded_size);
    if (err != ESP_OK) {
    	free(recorded_commands);
    	return err;
    }

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
    	free(recorded_commands);
    	return err;
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

/* Read from NVS and print restart counter
   and the table with run times.
   Return an error if anything goes wrong
   during this process.
 */
esp_err_t send_desired_command(char* b, char* m, char* f) {

    esp_err_t err = ESP_OK;

    char brand[15], model[15], func[15];

	snprintf(brand, COMMAND_STRUCT_STRING_LENGTH, "%s", b);
	snprintf(model, COMMAND_STRUCT_STRING_LENGTH, "%s",  m);
	snprintf(func, COMMAND_STRUCT_STRING_LENGTH, "%s", f);

    if (recorded_size == 0) {
    	ESP_LOGI(TAG_NVS, "Nothing saved yet!\n");
    	return ESP_FAIL;
    } else {
        ESP_LOGI(TAG_NVS, "REQUIRED SIZE: %d", recorded_size);
        ESP_LOGI(TAG_NVS, "SIZE OF COMMANDS: %d", sizeof(commands));
    	ESP_LOGI(TAG_NVS, "BRAND: %s", brand);
    	ESP_LOGI(TAG_NVS, "MODEL: %s", model);
    	ESP_LOGI(TAG_NVS, "FUNC: %s", func);
    	ESP_LOG_BUFFER_HEXDUMP(TAG_NVS, recorded_commands, recorded_size, 1);
        for (int i = 0; i < recorded_size/sizeof(commands); i++){
        	ESP_LOGI(TAG_NVS, "BRAND[%d]: %s", i, recorded_commands[i].brand);
        	ESP_LOGI(TAG_NVS, "MODEL[%d]: %s", i, recorded_commands[i].model);
        	ESP_LOGI(TAG_NVS, "FUNC[%d]: %s", i, recorded_commands[i].func);
        	if(strcmp(recorded_commands[i].brand,brand) == 0
     			   && strcmp(recorded_commands[i].model, model) == 0
     			   && strcmp(recorded_commands[i].func, func) == 0) {
        		ESP_LOGI(TAG_NVS, "ENCONTROU O COMANDO");
        		err = send_commands(&recorded_commands[i].rmt);
                if (err != ESP_OK) {
                	ESP_LOGI(TAG_NVS, "ERROR");
                	return err;
                }
                ESP_LOGI(TAG_NVS,"COMMAND SENT");
        	}
        }
    }
    return err;
}

static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    struct command *received_command;
    commands command;
    esp_err_t err;

    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                case UART_DATA:
                    ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    if (strcmp((char*)dtmp, "reset") == 0 ) {
                    	err = nvs_flash_erase();
                    	if (err != ESP_OK) {
                    		ESP_LOGI(TAG_NVS, "ERR0R TO ERASE: %s", esp_err_to_name(err));
                    	} else {
                    		ESP_LOGI(TAG_NVS, "ERASED");
                    	}
                    	break;
                    }
                    received_command = dtmp;
                    ESP_LOGD(TAG, "Command: %d", received_command->cmd);
                    if (received_command->cmd == REC) {
                    	ESP_LOGI(TAG, "RECCORDING");
                    	ESP_LOGI(TAG, "BRAND: %s", received_command->brand);
                    	ESP_LOGI(TAG, "MODEL: %s", received_command->model);
                    	ESP_LOGI(TAG, "FUNC: %s", received_command->func);
                    	command = receive_commands(received_command->brand, received_command->model, received_command->func);
                    	ESP_LOGI(TAG_NVS, "BRAND: %s", command.brand);
                    	ESP_LOGI(TAG_NVS, "MODEL: %s", command.model);
                    	ESP_LOGI(TAG_NVS, "FUNC: %s", command.func);
						err = save_command(command);
						if (err != ESP_OK) {
							ESP_LOGI(TAG_NVS, "Error (%s) saving command, please try again!\n", esp_err_to_name(err));
						} else {
							ESP_LOGI(TAG_NVS, "SAVED");
						}
                    } else if (received_command->cmd == SEND) {
                    	ESP_LOGI(TAG, "SENDING");
                    	ESP_LOGI(TAG, "BRAND: %s", received_command->brand);
                    	ESP_LOGI(TAG, "MODEL: %s", received_command->model);
                    	ESP_LOGI(TAG, "FUNC: %s", received_command->func);
                    	err = send_desired_command(received_command->brand, received_command->model, received_command->func);
                    	if (err != ESP_OK) {
							ESP_LOGI(TAG_NVS, "Error (%s) sending command, please try again!\n", esp_err_to_name(err));
						} else {
							ESP_LOGI(TAG_NVS, "SENT");
						}
                    } else {
                    	ESP_LOGI(TAG, "UNDEFINIED COMMAND");
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI(TAG, "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
	dtmp = NULL;
    vTaskDelete(NULL);
}

void app_main()
{
    rx_channels_init(RX_RMT_CHANNEL, RX_GPIO, 4);
	tx_channels_init(TX_RMT_CHANNEL, TX_GPIO, 4);

    esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
    ESP_ERROR_CHECK( err );

    err = get_commands();
    if (err != ESP_OK) {
    	ESP_ERROR_CHECK(err);
    }

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE , UART_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);

    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 8192, NULL, 12, NULL);
}
