.PHONY: lcdkube

lcdkube:
	gcc lcdkube.c -o lcdkube -lwiringPi -lwiringPiDev
