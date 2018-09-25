#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>


Load< MeshBuffer > tank_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("tank.pnc"));
});

Load< MeshBuffer > env_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("battlefield.pnc"));
});

Load< MeshBuffer > ball_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("ball.pnc"));;
});

Load< MeshBuffer::Mesh > ball_mesh(LoadTagDefault, []() {
	return &(ball_meshes->lookup("ball"));
});

Load< GLuint > tank_vao(LoadTagDefault, [](){
	return new GLuint(tank_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< GLuint > env_vao(LoadTagDefault, [](){
	return new GLuint(env_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< GLuint > ball_vao(LoadTagDefault, [](){
	return new GLuint(ball_meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *tank_transform = nullptr;
Scene::Transform *tank_cannon_transform = nullptr;
Scene::Transform *tank_top_transform = nullptr;
Scene::Transform *tank_bot_transform = nullptr;

Scene::Camera *camera = nullptr;

Scene *do_load_scene(std::string path, std::map<std::string, Scene::Transform **> transform_names, Scene::Camera **camera_ptr, const MeshBuffer *meshes, const GLuint *vao) {
	Scene *ret = new Scene;
	//load transform hierarchy:
	ret->load(data_path(path), [meshes, vao](Scene &s, Scene::Transform *t, std::string const *m_){
		Scene::Object *obj = s.new_object(t);

		obj->program = vertex_color_program->program;
		obj->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		obj->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		obj->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		if(m_) {
			// if this is an empty object (no mesh) then do nothing.
			std::string const &m = *m_;
			MeshBuffer::Mesh const &mesh = meshes->lookup(m);
			obj->empty = false;
			obj->vao = *vao;
			obj->start = mesh.start;
			obj->count = mesh.count;
		}
		
	});

	//look up transforms:
	for (Scene::Transform *t = ret->first_transform; t != nullptr; t = t->alloc_next) {
		auto search = transform_names.find(t->name);
		if(search != transform_names.end()) {
			if (*(search->second)) throw std::runtime_error("Multiple " + t->name + " transforms in scene.");
			*(search->second) = t;
		}
	}

	for(auto i = transform_names.begin(); i != transform_names.end(); ++i) {
		if(i->second == nullptr)
			throw new std::runtime_error("No " + i->first + " transforms in scene");
	}

	//look up the camera:
	if(camera_ptr) {
		for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
			if (c->transform->name == "Camera") {
				if (*camera_ptr) throw std::runtime_error("Multiple 'Camera' objects in scene.");
				*camera_ptr = c;
			}
		}
		if (!(*camera_ptr)) throw std::runtime_error("No 'Camera' camera in scene.");
	}
	

	return ret;
}

MLoad< Scene > tank_scene(LoadTagDefault, [](){
	std::map<std::string, Scene::Transform **> map;

	typedef std::pair<std::string, Scene::Transform **> pp;
	map.insert(pp("tank",   &tank_transform));
	map.insert(pp("top",    &tank_top_transform));
	map.insert(pp("bottom", &tank_bot_transform));
	map.insert(pp("cannon", &tank_cannon_transform));

	return do_load_scene("tank.scene", map, nullptr, &*tank_meshes, &*tank_vao);
});

MLoad< Scene > env_scene(LoadTagDefault, []() {
	std::map<std::string, Scene::Transform **> map;
	return do_load_scene("battlefield.scene", map, nullptr, &*env_meshes, &*env_vao);
});

GameMode::GameMode(Client &client_) : client(client_) {
	// Compile scene from loaded asset files
	full_scene = new Scene;
	full_scene->append_scene(std::move(*tank_scene));
	full_scene->append_scene(std::move(*env_scene));

	auto camera_transform = new Scene::Transform;
	camera_transform->position = glm::vec3(0, -5, 5);
	camera_transform->rotation = glm::quat(glm::vec3(glm::radians(45.f), 0, 0));
	camera = full_scene->new_camera(camera_transform);

	client.connection.send_raw("h", 1); //send a 'hello' to the server
}

GameMode::~GameMode() {
}

#include <iostream>

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_MOUSEMOTION) {
		const float twopi = 2.0f * glm::pi<float>();
		const float pitwo = 0.5f * glm::pi<float>();

		state.player_look.x += -(float)evt.motion.xrel / window_size.x * twopi;
		state.player_look.y += (float)evt.motion.yrel / window_size.x * twopi;
		state.player_look.x = fmod(state.player_look.x, twopi);
		state.player_look.y = glm::clamp(fmod(state.player_look.y, twopi), -pitwo + .05f, pitwo - .05f);

		// state.paddle.x = (evt.motion.x - 0.5f * window_size.x) / (0.5f * window_size.x) * Game::FrameWidth;
		// state.paddle.x = std::max(state.paddle.x, -0.5f * Game::FrameWidth + 0.5f * Game::PaddleWidth);
		// state.paddle.x = std::min(state.paddle.x,  0.5f * Game::FrameWidth - 0.5f * Game::PaddleWidth);
	}

	return false;
}

void GameMode::update(float elapsed) {
	state.update(elapsed);

	if (client.connection) {
		//send game state to server:
		client.connection.send_raw("s", 1);
		float d = 0;
		client.connection.send_raw(&d, sizeof(float));
	}

	client.poll([&](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			//probably won't get this.
		} else if (event == Connection::OnClose) {
			std::cerr << "Lost connection to server." << std::endl;
		} else { assert(event == Connection::OnRecv);
			std::cerr << "Ignoring " << c->recv_buffer.size() << " bytes from server." << std::endl;
			c->recv_buffer.clear();
		}
	});

	const Uint8* key_state = SDL_GetKeyboardState(NULL);

	if(key_state[SDL_SCANCODE_E]) {
		state.cannon_pitch += 0.5f * elapsed;
	}
	if(key_state[SDL_SCANCODE_Q]) {
		state.cannon_pitch -= 0.5f * elapsed;
	}
	state.cannon_pitch = glm::clamp(state.cannon_pitch, 0.f, .5f * glm::pi<float>());

	auto cam_rot = glm::quat(glm::vec3(state.player_look.y, state.player_look.x, 0));
	auto cam_rot_fwd = cam_rot * glm::vec3(0, 0, -1);

	auto tank_fwd = cam_rot_fwd;
	tank_fwd.y = 0;
	tank_fwd /= glm::length(tank_fwd);
	auto tank_right = glm::cross(tank_fwd, glm::vec3(0, 1, 0));

	glm::vec3 move(0,0,0);
	if(key_state[SDL_SCANCODE_W])
		move += tank_fwd;
	if(key_state[SDL_SCANCODE_S])
		move -= tank_fwd;
	if(key_state[SDL_SCANCODE_D])
		move += tank_right;
	if(key_state[SDL_SCANCODE_A])
		move -= tank_right;
	float move_sqlen = glm::dot(move, move);
	if(move_sqlen > .001f) {
		move /= sqrt(move_sqlen);
		state.player_last_fwd = move;
	}

	state.player_pos += move * 5.f * elapsed;

	float tank_ang = atan2(state.player_last_fwd.z, state.player_last_fwd.x) + glm::pi<float>() / 2;
	glm::quat bot_rot = glm::quat(glm::vec3(glm::pi<float>(), 0, 0));
	bot_rot *= glm::quat(0, sin(tank_ang / 2), 0, cos(tank_ang / 2));
	tank_bot_transform->rotation = bot_rot;

	// Convert logical state to things relevant for rendering
	camera->transform->rotation = cam_rot;
	camera->transform->position = state.player_pos + glm::vec3(0, 1.5f, 0) + (-cam_rot_fwd * 7.f);

	tank_top_transform->rotation = glm::quat(glm::vec3(0, state.player_look.x, 0));
	tank_cannon_transform->rotation = glm::quat(glm::vec3(state.cannon_pitch, 0, 0));

	tank_transform->position = state.player_pos;
}

void GameMode::draw(glm::uvec2 const &drawable_size) {
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.25f, 0.0f, 0.5f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	full_scene->draw(camera);

	GL_ERRORS();
}
