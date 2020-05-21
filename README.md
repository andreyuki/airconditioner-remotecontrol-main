#tfc-esp32-rmt-rxtx
Projeto para Trabalho de Fim de Curso de engenharia da computação da UNICAMP.

O projeto tem como finalidade criar um sistema capaz de controlar aparelhos de condicionamento de ar usando dispositivos IoT visando a economia de energia, através da utilização de um microcontrolador ESP32 e LEDs infraveremelho (sensor e atuador) para receber comandos de controles remotos de ar condicionado, armazená-los e então quando necessário enviá-los para os aparelhos.

Esse projeto tem como controle um script (controle.py) em pyhton informações do comando para o ESP32 via serial. No qual se pode entrar em modo de gravação para gravar os comandos do controle remoto ou modo de envio, no qual seleciona a função e o ESP32 envia o comando para o aparelho.

Para a parte de IR é utilizado o componente ir_tx_rx.

##Esquemático

Para a recepção do infravermelho são utilizados: um led sensor de infravermelho (TSOP1838D), um capacitor  de 10uF e um de 100nF, um resistor de 100 Ohm e um de 10k Ohm. 

Para o envio do infravermelho são utilizados: 3 led emissor de infravermelho (TSAL7300), um transistor NPN (BC337-25), um resistor de 22 Ohm e um de 68 Ohm

![Esquemático](./esquematico.png)

Os capacitores e resistores juntos ao sensor são utilizados para diminiur o ruído e o transistor junto aos emissores é para aumentar a corrente nos emissores.

Para o sensor está sendo utilizado a porta 21 e para o emissor a porta 15.

##Foto da placa

