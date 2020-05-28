# Main program

Responsible for the main control: integrating serial communication (UART), Non-volatile storage (NVS) and submodule ir_rx_tx

### UART
The UART was based on [esp-idf UART example](https://github.com/espressif/esp-idf/blob/master/examples/peripherals/uart/uart_events/main/uart_events_example_main.c).
The change was on the event treatment of the received valid data:

```C
case UART_DATA:
```

There are 3 commands:
1. Reset: clear all the saved commands in the NVS when the python script send the _"reset"_ command.

```C
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
```

2. Record: record a command when the python script send the the _"R"_ command by calling the [ir_rx_tx](https://github.com/andreyuki/ir_tx_rx) receive_commands function and then NVS save_command function.

```C
received_command = dtmp;
ESP_LOGD(TAG, "Command: %d", received_command->cmd);
if (received_command->cmd == REC) {
    command = receive_commands(received_command->brand, received_command->model, received_command->func);
    err = save_command(command);
    if (err != ESP_OK) {
        ESP_LOGI(TAG_NVS, "Error (%s) saving command, please try again!\n", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG_NVS, "SAVED");
    }
} 
```

3. Send: send a command when the python script send the _"W"_ command by calling the send_desired_command() function.

```C
else if (received_command->cmd == SEND) {
    err = send_desired_command(received_command->brand, received_command->model, received_command->func);
    if (err != ESP_OK) {
        ESP_LOGI(TAG_NVS, "Error (%s) sending command, please try again!\n", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG_NVS, "SENT");
    }
} 
```
* send_desired_command(char* b, char* m, char* f)
Function that receives the brand, model and command and verifies if the command is in the memory. If yes, call the [ir_rx_tx](https://github.com/andreyuki/ir_tx_rx) to send the command signal to the air conditioner.

  ```C
  esp_err_t send_desired_command(char* b, char* m, char* f) {

    esp_err_t err = ESP_OK;

    char brand[15], model[15], func[15];

	snprintf(brand, COMMAND_STRUCT_STRING_LENGTH, "%s", b);
	snprintf(model, COMMAND_STRUCT_STRING_LENGTH, "%s",  m);
	snprintf(func, COMMAND_STRUCT_STRING_LENGTH, "%s", f);

    if (recorded_size == 0) {
    	return ESP_FAIL;
    } else {
        for (int i = 0; i < recorded_size/sizeof(commands); i++){
        	if(strcmp(recorded_commands[i].brand,brand) == 0
     			   && strcmp(recorded_commands[i].model, model) == 0
     			   && strcmp(recorded_commands[i].func, func) == 0) {
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
  ```

### Non-volatile storage (NVS)
The ESP32 has a flash memory that was used for not losing the air conditioner commands when the esp32 is turned off. It was stored as a blob (binary large object).
Each command is a struct containing the air conditioner brand, model, command and the command signal itself:

```C
struct command {
	uint8_t cmd;
	char brand[15];
	char model[15];
	char func [15];
}__attribute__((packed));
```

First of all we initalize the NVS:
```C
esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK( err );
```

and then if there are any stored commands already, they are loaded to local memory:

```C
err = get_commands();
```

To read the values from the NVS, first we have to open the nvs and get the size:

```C
#get_commands()

err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);

// obtain required memory space to store blob being read from NVS (when 3rd parameter is NULL)
err = nvs_get_blob(my_handle, COMMANDS_KEY_NAME, NULL, &recorded_size);
```

and then memory is allocated and the commands in the nvs is saved locally:

```C
#get_commands()

if (recorded_size > 0) {
    recorded_commands = malloc(recorded_size);
    err = nvs_get_blob(my_handle, COMMANDS_KEY_NAME, recorded_commands, &recorded_size);
}
```

For saving commands the save_commands() function is used. Before saving command we check if the command is already saved by checking the brand, model and command name. If it is not found, the command is saved otherwise the command is saved local memory and in the nvs.

```C
if (numbers_of_commands > 0) {
    for (i = 0; i < numbers_of_commands; i++){
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
        memcpy(recorded_commands + numbers_of_commands, &command, sizeof(commands));
        recorded_size = new_size;
    }
} else {
    recorded_size = sizeof(commands);
    recorded_commands = malloc(recorded_size);
    memcpy(recorded_commands, &command, recorded_size);
}
// Write
err = nvs_set_blob(my_handle, COMMANDS_KEY_NAME, recorded_commands, recorded_size);
if (err != ESP_OK) {
    free(recorded_commands);
    return err;
}
```
After writing in the nvs, it must call the function _"nvs_commit()"_

```C
// Commit written value.
// After setting any values, nvs_commit() must be called to ensure changes are written
// to flash storage. Implementations may write to storage at other times,
// but this is not guaranteed.
err = nvs_commit(my_handle);
```
