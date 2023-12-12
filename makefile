all:
	touch src/tmp/dummy.txt
	rm src/tmp/*	
	gcc src/main.c -o src/main
	gcc src/server.c src/include/log.c -o src/server
	gcc src/userinterface.c src/include/log.c -lncurses -o src/userinterface
	gcc src/drone.c src/include/log.c -o src/drone
	gcc src/watchdog.c src/include/log.c -o src/watchdog
	touch src/tmp/dummy.txt
	rm src/tmp/*	
	gcc src/main.c src/include/log.c -o src/main
	gcc src/server.c src/include/log.c -o src/server
	gcc src/userinterface.c src/include/log.c -lncurses -o src/userinterface
	gcc src/drone.c src/include/log.c -o src/drone
	gcc src/watchdog.c src/include/log.c -o src/watchdog
	sleep 2
	./src/main

	

