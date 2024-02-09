all:
	g++ src/*.cpp -Og -g -o jj_http_server

clean:
	tcpkill -i wlp4s0 port 25565
