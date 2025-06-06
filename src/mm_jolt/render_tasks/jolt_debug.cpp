#include "./jolt_debug.hpp"

#include <mm/services/scene_service_interface.hpp>
#include <mm/fs_const_archiver.hpp>

#include <mm/opengl/camera_3d.hpp>
#include <mm/opengl/buffer.hpp>
#include <mm/opengl/render_tasks/fast_sky_render_task.hpp> // fsc

#include <entt/entity/registry.hpp>

#include <glm/trigonometric.hpp>
#include <glm/geometric.hpp>

#include <Jolt/Jolt.h>

#include "../utils.hpp"
#include "../components/jolt_context.hpp"

#include <memory>
#include <cstdint>

#include <iostream>

namespace mm_jolt::OpenGL::RenderTasks {

JoltDebug::JoltDebug(MM::Engine& engine) {
	//_default_context.draw_settings.mDrawShapeWireframe = true;
	_default_context.draw_settings.mDrawBoundingBox = true;
	_default_context.draw_settings.mDrawCenterOfMassTransform = true;
	_default_context.draw_settings.mDrawVelocity = true;
	setupShaderFiles();

	// line shader
	_line_shader = MM::OpenGL::Shader::createF(engine, vertexPathLine, fragmentPathLine);
	assert(_line_shader != nullptr);
	_line_vao = std::make_unique<MM::OpenGL::VertexArrayObject>();

	_triangle_shader = MM::OpenGL::Shader::createF(engine, vertexPathTriangle, fragmentPathTriangle);
	assert(_triangle_shader != nullptr);
	_triangle_vao = std::make_unique<MM::OpenGL::VertexArrayObject>();
}

JoltDebug::~JoltDebug(void) {
}

void JoltDebug::render(MM::Services::OpenGLRenderer& rs, MM::Engine& engine) {
	auto* ssi = engine.tryService<MM::Services::SceneServiceInterface>();
	if (
		ssi == nullptr ||
		!ssi->getScene().ctx().contains<mm_jolt::Components::JoltContext>() ||
		!ssi->getScene().ctx().contains<MM::OpenGL::Camera3D>()
	) {
		return;
	}

	const auto& ddc = _default_context;

	if (!ddc.enable) {
		return;
	}

	rs.targets[target_fbo]->bind(MM::OpenGL::FrameBufferObject::RW);

	const auto& gcam = ssi->getScene().ctx().get<MM::OpenGL::Camera3D>();
	SetCameraPos(to_jph(gcam.translation)); // jph rend update
	_global_pos = gcam.translation;

	// copy
	auto cam = gcam;
	cam.translation = {0, 0, 0};
	cam.updateView();
	_vp = cam.getViewProjection();
	//cam.getFrustumPlanes(); // TODO: cull?

	_triangle_shader->bind();
	if (ddc.shade_sun && ssi->getScene().ctx().contains<MM::OpenGL::RenderTasks::FastSkyContext>()) {
		const auto& fsc = ssi->getScene().ctx().get<MM::OpenGL::RenderTasks::FastSkyContext>();
		//sky_ctx.fsun.y = glm::sin(fsc.time * 0.01f);
		//sky_ctx.fsun.z = glm::cos(fsc.time * 0.01f);
		glm::vec3 sun_dir = glm::normalize(glm::vec3{-0.33f, -glm::cos(fsc.time * 0.01f), glm::sin(fsc.time * 0.01f)} * -1.f);

		const float sun_height = glm::max(-sun_dir.z, 0.f);
		const float sun_light_intensity = glm::pow(sun_height, 1.4);

		//const glm::vec3 ambient_color {0.25f, 0.25f, 0.5f};
		const glm::vec3 ambient_color {0.4f, 0.4f, 0.4f};
		_triangle_shader->setUniform3f("g_ambient", ambient_color * (1.f + sun_light_intensity*0.2f));

		_triangle_shader->setUniform3f("g_directional_light_dir", glm::normalize(sun_dir));

		//const glm::vec3 sun_color{1.5f, 1.4f, 0.7f};
		const glm::vec3 sun_color{1.0f, 1.0f, 1.0f};
		_triangle_shader->setUniform3f("g_directional_light_color", sun_color * sun_light_intensity);
	} else { // fallback
		_triangle_shader->setUniform3f("g_ambient", glm::vec3{0.4f});
		_triangle_shader->setUniform3f("g_directional_light_dir", glm::normalize(glm::vec3{0.33, 1.0, -1.0}));
		_triangle_shader->setUniform3f("g_directional_light_color", glm::vec3{1.2});
	}

	auto& jc = ssi->getScene().ctx().get<mm_jolt::Components::JoltContext>();

	jc.physics_system.DrawBodies(ddc.draw_settings, this);

	if (!_triangle_cbuf.empty()) {
		renderTriangles();
		_triangle_cbuf.clear();
	}

	if (!_line_cbuf.empty()) {
		renderLines();
		_line_cbuf.clear();
	}

	if (ddc.draw_constraints) {
		jc.physics_system.DrawConstraints(this);
	}
}

void JoltDebug::renderLines(void) {
	// its lines <.<
	//glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);

	glDisable(GL_DEPTH_TEST); // for lines anyway

	// TODO: alpha?
	//glEnable(GL_BLEND);
	glDisable(GL_BLEND);

	_line_shader->bind();
	_line_shader->setUniformMat4f("g_vp", _vp);
	_line_vao->bind();

	// TODO: temp bad idea?
	MM::OpenGL::Buffer lineb{
		_line_cbuf.data(),
		(_line_cbuf.size() * sizeof(float) * 3 + _line_cbuf.size() * sizeof(uint32_t)) * 2,
		GL_DYNAMIC_DRAW
	};
	lineb.bind();

	// something is still off

	//layout (location = 0) in vec3 _vert_pos;
	//layout (location = 1) in uint _vert_color;

	// line buffer layout
	// vec3 (12bytes)
	// uint32 (4bytes)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Line)/2, (void*)offsetof(Line, from_pos));
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(Line)/2, (void*)offsetof(Line, from_color));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_LINES, 0, _line_cbuf.size()*2);

	lineb.unbind();

	_line_vao->unbind();
}

void JoltDebug::renderTriangles(void) {
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	_triangle_shader->bind();
	_triangle_shader->setUniformMat4f("g_vp", _vp);
	_triangle_vao->bind();

	// TODO: temp bad idea?
	MM::OpenGL::Buffer trib{
		_triangle_cbuf.data(),
		(_triangle_cbuf.size() * sizeof(float) * 6 + _triangle_cbuf.size() * sizeof(uint32_t)) * 3,
		GL_DYNAMIC_DRAW
	};
	trib.bind();

	//layout (location = 0) in vec3 _vert_pos;
	//layout (location = 1) in vec3 _vert_norm;
	//layout (location = 2) in uint _vert_color;

	// line buffer layout
	// vec3 (12bytes)
	// vec3 (12bytes)
	// uint32 (4bytes)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MyTriangle)/3, (void*)offsetof(MyTriangle, v0_pos));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MyTriangle)/3, (void*)offsetof(MyTriangle, v0_norm));
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(MyTriangle)/3, (void*)offsetof(MyTriangle, v0_color));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, _triangle_cbuf.size()*3);

	trib.unbind();

	_triangle_vao->unbind();
}

void JoltDebug::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) {
	// make relative to cam
	glm::vec3 from = to_glm(inFrom) - _global_pos;
	glm::vec3 to = to_glm(inTo) - _global_pos;

	_line_cbuf.emplace_back(from, inColor.GetUInt32(), to, inColor.GetUInt32());
}

void JoltDebug::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) {
	// TODO: shadow?
	(void)inCastShadow;

	glm::vec3 v0 = to_glm(inV1) - _global_pos;
	glm::vec3 v1 = to_glm(inV2) - _global_pos;
	glm::vec3 v2 = to_glm(inV3) - _global_pos;

	glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

	_triangle_cbuf.emplace_back(
		v0, normal, inColor.GetUInt32(),
		v1, normal, inColor.GetUInt32(),
		v2, normal, inColor.GetUInt32()
	);
}

void JoltDebug::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) {
	(void)inPosition, (void)inString, (void)inColor, (void)inHeight;
}

void JoltDebug::setupShaderFiles(void) {
	FS_CONST_MOUNT_FILE(vertexPathLine,
GLSL_VERSION_STRING
R"(
layout (location = 0) in vec3 _vert_pos;
layout (location = 1) in uint _vert_color;

uniform mat4x4 g_vp;

out vec3 _color;

void decompress_color(in uint col, out vec3 color) {
	// TODO: check packing
	color.r = float(col & 255u);
	color.g = float((col >> 8u) & 255u);
	color.b = float((col >> 16u) & 255u);
	color /= 255;
}

void main() {
	decompress_color(_vert_color, _color);

	gl_Position = g_vp * vec4(_vert_pos, 1.0);
}
)")

	FS_CONST_MOUNT_FILE(fragmentPathLine,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

in vec3 _color;

layout (location = 0) out vec3 _out_color;

void main() {
	_out_color = _color;
}
)")

	FS_CONST_MOUNT_FILE(vertexPathTriangle,
GLSL_VERSION_STRING
R"(
layout (location = 0) in vec3 _vert_pos;
layout (location = 1) in vec3 _vert_norm;
layout (location = 2) in uint _vert_color;

uniform mat4x4 g_vp;

out vec3 _norm;
out vec3 _color;

void decompress_color(in uint col, out vec3 color) {
	// TODO: check packing
	color.r = float(col & 255u);
	color.g = float((col >> 8u) & 255u);
	color.b = float((col >> 16u) & 255u);
	color /= 255;
}

void main() {
	decompress_color(_vert_color, _color);

	gl_Position = g_vp * vec4(_vert_pos, 1.0);

	_norm = _vert_norm;
}
)")

	FS_CONST_MOUNT_FILE(fragmentPathTriangle,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

uniform vec3 g_ambient;
uniform vec3 g_directional_light_dir;
uniform vec3 g_directional_light_color;

in vec3 _norm;
in vec3 _color;

layout (location = 0) out vec3 _out_color;

vec3 directional_lighting(in vec3 normal, in vec3 light_direction, in vec3 light_color) {
	return vec3(max(0.0, dot(-light_direction, normal))) * light_color;
}

void main() {
	vec3 lighting = g_ambient; // ambient

	lighting += directional_lighting(_norm, g_directional_light_dir, g_directional_light_color);

	_out_color = _color * lighting;
}
)")

}

} // mm_jolt::OpenGL::RenderTasks

