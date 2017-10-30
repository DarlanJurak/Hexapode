all:
	g++ ./src/hexapode_brain.cpp -o ./bin/hexapode_brain `pkg-config --cflags --libs opencv` -lwiringPi
clean:
	#rm -f server client
