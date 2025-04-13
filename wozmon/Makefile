wozmon.bin: wozmon.o
	ld65 -C wozmon.cfg -o wozmon.bin wozmon.o
wozmon.o: wozmon.s
	ca65 -o wozmon.o wozmon.s

.PHONY: clean
clean:
	rm *.bin *.o