#pragma once

#include <glm/glm.hpp>

enum Player {
	PLAYER_1 = 1, PLAYER_2 = 2
};

struct Projectile {
	glm::vec3 initial_pos;
	glm::vec3 initial_vel;
	float fire_time;
	Player origin;
};

struct Game {
	// Player view direction in radians (yaw, pitch)
	glm::vec2 player_look = glm::vec2(0, 0);
	// Player position in worldspace
	glm::vec3 player_pos = glm::vec3(0, 0, 0);
	// Last recorded player move direction
	glm::vec3 player_last_fwd = glm::vec3(0, 0, 1);
	// Cannon pitch in radians, range [0, pi/2]
	float cannon_pitch = 0;

	void update(float time);

	static constexpr const float FrameWidth = 10.0f;
	static constexpr const float FrameHeight = 8.0f;
	static constexpr const float PaddleWidth = 2.0f;
	static constexpr const float PaddleHeight = 0.4f;
	static constexpr const float BallRadius = 0.5f;
};
