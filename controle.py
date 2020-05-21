# -*- coding: utf-8 -*-
import sys
import serial
import time
from serial.tools import list_ports

ser = serial.Serial()

def interface_controle():
	print ("#####################################")
	print ("        --------------------")
	print ("        |                  |")
	print ("        |   ---      ---   |")
	print ("        |   |1|      |2|   |")
	print ("        |   ---      ---   |")
	print ("        |                  |")
	print ("        |   ---            |")
	print ("        |   |3|            |")
	print ("        |   ---            |")
	print ("        |                  |")
	print ("        --------------------")
	print ("1 : On/Off")
	print ("2 : Água")
	print ("3 : Velocidade")

def menu_principal():
	interface_controle()
	print ("8 : Resetar memória")
	print ("9 : Gravar comando")
	print ("0 : Sair")

def choose_type():
	type = input("Digite\n -1 para Trocador\n -2 para Ar Condicionado:\n")
	return type

def send_command(type, rw, func):
	if type == 1:
		brand = "ecobrisa"
		model = "EB-35"
	brand = brand.ljust(15, '0')
	print (brand)
	model = model.ljust(15, '0')
	print (model)
	func = func.ljust(15, '0')
	print(func)
	command = rw + brand + model + func
	print(command)
	ser.write(command)

def gravacao(type):
	while True:
		interface_controle()
		comando = input ("Qual comando deseja gravar? Digite 0 para sair. \nDigite o comando: ")
		if comando == 1:
			print ("Aperte o botão de ligar.")
			send_command(type, "R", "onoff")
			time.sleep(1)
		elif comando == 2:
			print ("Aperte o botão de água.")
			send_command(type, "R", "water")
			time.sleep(1)
		elif comando == 3:
			print ("Aperte o botão de velocidade.")
			send_command(type, "R", "speed")
			time.sleep(1)
		elif comando == 0:
			return

def on_off(type):
	send_command(type, "W", "onoff")
	print ("Comando enviado")

def water(type):
	send_command(type, "W", "water")
	print ("Comando enviado")

def speed(type):
	send_command(type, "W", "speed")
	print ("Comando enviado")

def save_commands():
	ser.write("S")
	print ("Comandos salvos")

def main():
	for p in list_ports.comports():
		print (p)
		if "FTDI" in p.manufacturer:
			port = p.device
			print (port)

	ser.baudrate = 115200
	ser.port = port
	ser.open()

	#type = choose_type();
	type = 1

	while True:
		menu_principal()
		comando = input("Digite comando: ")
		if comando == 1:
			on_off(type)
		elif comando == 2:
			water(type)
		elif comando == 3:
			speed(type)
		elif comando == 8:
			ser.write("reset")
		elif comando == 9:
			gravacao(type)
		elif comando == 0:
			save_commands()
			print ("Saindo...")
			sys.exit()


if __name__ == "__main__":
    main()
