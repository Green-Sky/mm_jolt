#pragma once

#include <mm/opengl/render_task.hpp>
#include <mm/services/opengl_renderer.hpp>
#include <mm/opengl/shader.hpp>
#include <mm/opengl/vertex_array_object.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyManager.h>

#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRendererSimple.h>
#else
	// Hack to still compile DebugRenderer inside the test framework when Jolt is compiled without
	#define JPH_DEBUG_RENDERER
	// Make sure the debug renderer symbols don't get imported or exported
	#define JPH_DEBUG_RENDERER_EXPORT
	#include <Jolt/Renderer/DebugRendererSimple.h>
	#undef JPH_DEBUG_RENDERER
	#undef JPH_DEBUG_RENDERER_EXPORT
#endif

#include <glm/vec3.hpp>

#include <memory>

namespace mm_jolt::OpenGL::RenderTasks {

struct DebugDrawContext {
	// the different debug draw options
	bool enable{true};

	JPH::BodyManager::DrawSettings draw_settings{};
	bool draw_constraints{true};

	// TODO: bool lit{true};
	bool shade_sun{true};
};

// basically vec4 * 2
// split into line segments? (points)
struct Line {
	glm::vec3 from_pos;
	uint32_t from_color;

	glm::vec3 to_pos;
	uint32_t to_color;
};
static_assert(sizeof(Line) == sizeof(glm::vec3)*2 + sizeof(uint32_t)*2);

struct MyTriangle {
	glm::vec3 v0_pos;
	glm::vec3 v0_norm;
	uint32_t v0_color;

	glm::vec3 v1_pos;
	glm::vec3 v1_norm;
	uint32_t v1_color;

	glm::vec3 v2_pos;
	glm::vec3 v2_norm;
	uint32_t v2_color;
};
static_assert(sizeof(MyTriangle) == sizeof(glm::vec3)*6 + sizeof(uint32_t)*3);

class JoltDebug : public MM::OpenGL::RenderTask, public JPH::DebugRendererSimple {
	private:
		// fallback, used if the scene does not provide one
		DebugDrawContext _default_context;

	private:
		//MM::Services::OpenGLRenderer* rs{nullptr};
		glm::vec3 _global_pos;
		glm::mat4 _vp;
		std::shared_ptr<MM::OpenGL::Shader> _line_shader;
		std::unique_ptr<MM::OpenGL::VertexArrayObject> _line_vao;
		std::vector<Line> _line_cbuf;

		std::shared_ptr<MM::OpenGL::Shader> _triangle_shader;
		std::unique_ptr<MM::OpenGL::VertexArrayObject> _triangle_vao;
		std::vector<MyTriangle> _triangle_cbuf;

	public:
		explicit JoltDebug(MM::Engine&);
		~JoltDebug(void);

		const char* name(void) override { return "JoltDebug"; }

		void render(MM::Services::OpenGLRenderer& rs, MM::Engine& engine) override;

		void renderLines(void);
		void renderTriangles(void);

	public: // jph renderer interface
		void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
		void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;
		void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override;

	public:
		const char* vertexPathLine = "shader/jolt_debug_render_task/line_vert.glsl";
		const char* fragmentPathLine = "shader/jolt_debug_render_task/line_frag.glsl";
		const char* vertexPathTriangle = "shader/jolt_debug_render_task/triangle_vert.glsl";
		const char* fragmentPathTriangle = "shader/jolt_debug_render_task/triangle_frag.glsl";

		std::string target_fbo = "display";

	private:
		void setupShaderFiles(void);

};

} // mm_jolt::OpenGL::RenderTasks

