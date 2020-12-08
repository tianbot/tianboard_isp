tianboard_isp: main.c wiringSerial.c isp.c
	gcc main.c wiringSerial.c isp.c -Iinclude -o tianboard_isp

clean:
	@rm tianboard_isp

