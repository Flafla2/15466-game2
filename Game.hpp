#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <chrono>

enum Player {
	HOST_PLAYER = 0,
	OTHER_PLAYER = 1
};

struct Projectile {
	glm::vec3 initial_pos = glm::vec3(0,0,0);
	glm::vec3 initial_vel = glm::vec3(0,0,0);
	float fire_time = 0;
	Player origin = Player::HOST_PLAYER;

	glm::vec3 get_position(float t) { // t = time after firing
		return glm::vec3(initial_vel.x, initial_vel.y - 0.5f * 9.8f * t, initial_vel.z) * t;
	}
};

struct Tank {
	// Player view direction in radians (yaw, pitch)
	glm::fvec2 look = glm::fvec2(0, 0);
	// Player position in worldspace
	glm::fvec3 pos = glm::fvec3(0, 0, 0);
	// Last recorded player move direction
	glm::fvec3 last_fwd = glm::fvec3(0, 0, 1);
	// Cannon pitch in radians, range [0, pi/2]
	float cannon_pitch = 0;
	// In s since game start
	float last_fire_time = 0;
	// Player that owns this tank
	Player owner;

	Tank(Player owner) : owner(owner) {}
};

static_assert(sizeof(Tank) == 8 + 12 + 12 + 4 + 4 + 4, "Tank is packed");
static_assert(sizeof(Projectile) == 12 + 12 + 4 + 4, "Projectile is packed");

struct Game {
	Tank host = Tank(HOST_PLAYER);
	Tank other = Tank(OTHER_PLAYER);

	float time_sync_net;
	std::chrono::time_point<std::chrono::steady_clock> time_sync_loc;

	std::vector<Projectile> projectiles;

	void update(float time);

	static constexpr const float FrameWidth = 10.0f;
	static constexpr const float FrameHeight = 8.0f;
	static constexpr const float PaddleWidth = 2.0f;
	static constexpr const float PaddleHeight = 0.4f;
	static constexpr const float BallRadius = 0.5f;
};
