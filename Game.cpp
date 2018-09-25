#include "Game.hpp"

#include <iostream>

void Game::update(float time) {

	auto cur_time = std::chrono::steady_clock::now();
	float net_time = (cur_time - time_sync_loc).count() + time_sync_net;
	
	for(auto p_ = projectiles.begin(); p_ != projectiles.end(); ) {
		auto p = p_; ++p_;

		glm::vec3 pos = p->get_position(net_time - p->fire_time);
		std::cout << pos.x << " " << pos.y << " " << pos.z << std::endl;

		//TODO: collision detection
		if (pos.y < 0) {
			// destroy
			//projectiles.erase(p);
		}

	}

}
