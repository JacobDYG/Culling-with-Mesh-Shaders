#include <glm/gtc/type_ptr.hpp>
#include <nell/components/transform.hpp>
#include <nell/components/triangle_mesh.hpp>
#include <nell/components/triangle_mesh_draw_elements_objects.hpp>
#include <nell/definitions.hpp>
#include <nell/scenes/basic_scene.hpp>
#include <nell/systems/model_import_system.hpp>
#include <utils/gl_utils.hpp>

static constexpr unsigned int kPipelineCount = 1;
static constexpr GLuint kTestPipeline = 0;

static constexpr auto kAttrPosition = "position";
static constexpr auto kAttrNormal = "normal";
static constexpr auto kUniformMvp = "mvp";
static constexpr unsigned int kShaderProgramCount = 1;

static constexpr auto kVerticesBindingIndex = 0;
static constexpr auto kNormalBindingIndex = 1;

namespace nell
{
BasicScene::BasicScene()
    : _basic_vertex_shader(
          0, GL_VERTEX_SHADER,
          std::string(kShaderPath) + std::string("basic_shader.vert")),
      _basic_fragment_shader(
          0, GL_FRAGMENT_SHADER,
          std::string(kShaderPath) + std::string("basic_shader.frag"))
{
  _resource_location_maps.reserve(kShaderProgramCount);

  glCreateProgramPipelines(kPipelineCount, _program_pipeline);
  glUseProgramStages(_program_pipeline[kTestPipeline], GL_VERTEX_SHADER_BIT,
                     static_cast<GLuint>(_basic_vertex_shader));
  glUseProgramStages(_program_pipeline[kTestPipeline], GL_FRAGMENT_SHADER_BIT,
                     static_cast<GLuint>(_basic_fragment_shader));

  _position_attr_loc =
      _basic_vertex_shader.getProgramInput(kAttrPosition).location;
  _normal_attr_loc = _basic_vertex_shader.getProgramInput(kAttrNormal).location;

  _uniform_mvp_loc = _basic_vertex_shader.getUniform(kUniformMvp).location;
}

void BasicScene::populate(Scene *scene, entt::registry &reg)
{
  {
    auto entity = reg.create();
    reg.assign<comp::EntityName>(entity, "Armadillo");
    auto &asp = reg.assign<comp::ModelSource>(entity);
    auto &tf = reg.assign<comp::Transform>(entity);
    asp.path = "Armadillo.ply";
  }

  {
    auto entity = reg.create();
    reg.assign<comp::EntityName>(entity, "Sponza");
    auto &asp = reg.assign<comp::ModelSource>(entity);
    auto &tf = reg.assign<comp::Transform>(entity);
    asp.path = "sponza.obj";
  }

  systems::importModelsFromSource<comp::TriangleMesh>(reg);
}

void BasicScene::setup(Scene *scene, entt::registry &reg)
{
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // ### Setup VAO and Mesh Buffers ###
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Setup VAO
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  glCreateVertexArrays(1, &_vertex_array_object);
  // Enable vao attributes
  glEnableVertexArrayAttrib(_vertex_array_object, _position_attr_loc);
  glEnableVertexArrayAttrib(_vertex_array_object, _normal_attr_loc);
  // Format vao attributes
  glVertexArrayAttribFormat(_vertex_array_object, _position_attr_loc,
                            comp::TriangleMesh::getVertexSize(),
                            comp::TriangleMesh::getVertexType(), GL_FALSE, 0);
  glVertexArrayAttribFormat(_vertex_array_object, _normal_attr_loc,
                            comp::TriangleMesh::getNormalSize(),
                            comp::TriangleMesh::getNormalType(), GL_FALSE, 0);

  // Set binding location of attributes
  glVertexArrayAttribBinding(_vertex_array_object, _position_attr_loc,
                             kVerticesBindingIndex);
  glVertexArrayAttribBinding(_vertex_array_object, _normal_attr_loc,
                             kNormalBindingIndex);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  auto mesh_view = reg.view<comp::TriangleMesh>();

  for (auto entity : mesh_view)
  {
    auto &triangle_mesh = mesh_view.get<comp::TriangleMesh>(entity);
    auto &draw_elements_buffer =
        reg.assign<comp::TriangleMeshDrawElementsObjects>(entity);

    auto &vbo = draw_elements_buffer.vertex_buffer_object;
    auto &nbo = draw_elements_buffer.normal_buffer_object;
    auto &ibo = draw_elements_buffer.index_buffer_object;

    // Setup VBO
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, triangle_mesh.getVerticesSize(),
                         triangle_mesh.vertices.data(), 0);
    // Setup NBO
    glCreateBuffers(1, &nbo);
    glNamedBufferStorage(nbo, triangle_mesh.getNormalsSize(),
                         triangle_mesh.normals.data(), 0);

    // Setup IBO
    glCreateBuffers(1, &ibo);
    glNamedBufferStorage(ibo, triangle_mesh.getIndicesSize(),
                         triangle_mesh.indices.data(), 0);
    //// Bind indexbuffer to vao
    // glVertexArrayElementBuffer(_vertex_array_object, ibo);
  }
}
void BasicScene::resize(int w, int h) {}

void BasicScene::update(Scene *scene, entt ::registry &reg,
                        const input::NellInputList &input_list,
                        const double &time, const double &delta_time)
{
  scene->drawComponentImGui<comp::TriangleMesh,
                            comp::TriangleMeshDrawElementsObjects>();
}
void BasicScene::render(Scene *scene, entt::registry &reg,
                        entt::entity &camera_entity, const double &time,
                        const double &delta_time)
{
  auto perspective_camera = reg.get<comp::PerspectiveCamera>(camera_entity);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // ### Render Meshes with Basic Shader ###
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Get Entities with Model of BasisShadingMesh and iterate
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  auto render_objects =
      reg.view<comp::TriangleMesh, comp::TriangleMeshDrawElementsObjects,
               comp::Transform>();
  const auto vertex_size_t = comp::TriangleMesh::getVertexSizeT();
  const auto normal_size_t = comp::TriangleMesh::getNormalSizeT();

  glUseProgram(0);  // Unbind any bind programs to use program pipeline instead.

  for (auto entity : render_objects)
  {
    auto [triangle_mesh, gpu_buffers, transform] =
        render_objects
            .get<comp::TriangleMesh, comp::TriangleMeshDrawElementsObjects,
                 comp::Transform>(entity);

    auto mvp_matrix = perspective_camera.getViewProjectionMatrix() *
                      transform.getTransformation();
    auto mvp_matrix_ptr = glm::value_ptr(mvp_matrix);

    glBindProgramPipeline(_program_pipeline[kTestPipeline]);

    auto &vbo = gpu_buffers.vertex_buffer_object;
    auto &nbo = gpu_buffers.normal_buffer_object;
    auto &ibo = gpu_buffers.index_buffer_object;

    // Bind vbo to vao at bindloc
    glVertexArrayVertexBuffer(_vertex_array_object, kVerticesBindingIndex, vbo,
                              0, vertex_size_t);
    // Bind nbo to vao at bindloc
    glVertexArrayVertexBuffer(_vertex_array_object, kNormalBindingIndex, nbo, 0,
                              normal_size_t);
    // Bind indexbuffer to vao
    glVertexArrayElementBuffer(_vertex_array_object, ibo);

    glProgramUniformMatrix4fv(static_cast<GLuint>(_basic_vertex_shader),
                              _uniform_mvp_loc, 1, GL_FALSE, mvp_matrix_ptr);

    glBindVertexArray(_vertex_array_object);
    // Draw Call
    glDrawElements(GL_TRIANGLES, triangle_mesh.indices.size(), GL_UNSIGNED_INT,
                   nullptr);
  }
}
}  // namespace nell
