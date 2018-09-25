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
			for(auto connection = server.connections.begin(); connection != server.connections.end(); ++connection) {
				Tank dd = f ? state.other : state.host;
				dd.owner = OTHER_PLAYER;

				connection->send_raw("t", 1);
				connection->send(dd);

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
						return; //wait for more data
					}

					Tank *dest;

					bool first = true;
					for(auto connection = server.connections.begin(); connection != server.connections.end(); ++connection) {
						if(connection->socket == c->socket) {
							std::vector< char >& buf = connection->recv_buffer;

							dest = first ? &state.host : &state.other;
							memcpy(dest, ((char*)(buf.data())) + 1, sizeof(Tank));
							buf.erase(buf.begin(), buf.begin() + 1 + sizeof(Tank));

							dest->owner = first ? HOST_PLAYER : OTHER_PLAYER;
							std::cout << "tank: pos " << (float)(dest->pos.x) << ", " << (float)(dest->pos.y) << ", " << dest->pos.z << " look " << dest->look.x << ", " << dest->look.y << std::endl;
						}
						first = false;
					}
				}
			}
		}, 0.01);


	}
}
