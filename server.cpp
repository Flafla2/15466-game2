#include "Connection.hpp"
#include "Game.hpp"

#include <iostream>
#include <set>
#include <chrono>

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}
	
	Server server(argv[1]);

	Game state;

	while (1) {
		static auto then = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		if (now > then + std::chrono::milliseconds(300)) {
			bool f = true;
			for(auto cur_c : server.connections) {
				Tank dd = f ? state.host : state.other;
				dd.owner = OTHER_PLAYER;

				cur_c.send_raw("t", 1);
				cur_c.send(dd);
				//std::cout << "SEND " << f << std::endl;
				f = false;
			}
			then = now;
		}
		
		server.poll([&](Connection *c, Connection::Event evt){
			if (evt == Connection::OnOpen) {

				if(server.connections.size() > 2) {
					std::cout << "User connected but there are too many connections.  Ignoring." << std::endl;
					c->close();
				} else {
					c->send_raw("h", 1);
				}

			} else if (evt == Connection::OnClose) {
			} else { assert(evt == Connection::OnRecv);
				if (c->recv_buffer[0] == 'h') {
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
					std::cout << c << ": Got hello." << std::endl;
				} else if (c->recv_buffer[0] == 't') {
					if (c->recv_buffer.size() < 1 + sizeof(Tank)) {
						std::cout << "waiting for t..." << std::endl;
						return; //wait for more data
					}

					Tank *dest;

					bool first = true;
					for(auto cur_c : server.connections) {
						if(cur_c.socket == c->socket) {
							dest = first ? &state.host : &state.other;
							memcpy(dest, c->recv_buffer.data() + 1, sizeof(Tank));
							dest->owner = first ? HOST_PLAYER : OTHER_PLAYER;
						}
						first = false;
					}

					//TODO: Processing before it is blasted back?
				}
			}
		}, 0.01);
	}
}
