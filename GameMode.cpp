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


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("tank.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *tank_transform = nullptr;
Scene::Transform *tank_top_transform = nullptr;
Scene::Transform *tank_bot_transform = nullptr;

Scene::Camera *camera = nullptr;

Scene *do_load_scene(std::string path, std::map<std::string, Scene::Transform **> transform_names, Scene::Camera **camera_ptr) {
	Scene *ret = new Scene;
	//load transform hierarchy:
	ret->load(data_path(path), [](Scene &s, Scene::Transform *t, std::string const *m_){
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
			obj->vao = *meshes_for_vertex_color_program;
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
	map.insert(pp("tank",     &tank_transform));
	map.insert(pp("tank_top", &tank_top_transform));
	map.insert(pp("tank_bot", &tank_bot_transform));

	return do_load_scene("tank.scene", map, nullptr);
});

GameMode::GameMode(Client &client_) : client(client_) {
	auto camera_transform = new Scene::Transform;
	camera_transform->position = glm::vec3(0, -5, 5);
	camera_transform->rotation = glm::quat(glm::vec3(glm::radians(45.f), 0, 0));
	camera = new Scene::Camera(camera_transform);

	// Compile scene from loaded asset files
	full_scene = new Scene;
	full_scene->append_scene(std::move(*tank_scene));

	client.connection.send_raw("h", 1); //send a 'hello' to the server
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	if (evt.type == SDL_MOUSEMOTION) {
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
		client.connection.send_raw(&state.paddle.x, sizeof(float));
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

	//copy game state to scene positions:
	// ball_transform->position.x = state.ball.x;
	// ball_transform->position.y = state.ball.y;

	// paddle_transform->position.x = state.paddle.x;
	// paddle_transform->position.y = state.paddle.y;
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
