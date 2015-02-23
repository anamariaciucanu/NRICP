#include "shader.h"
#include "load_functions.h"



Shader::Shader(const char *_fileVertexShader, const char *_fileFragmentShader)
{
 m_logger = Logger::getInstance();
 loadVertexShader(_fileVertexShader);
 loadFragmentShader(_fileFragmentShader);
 createShaderProgramme();
}

Shader::~Shader()
{


}

bool Shader::loadVertexShader(const char* _fileVertexShader)
{
 const char* codeVertexShader = file_read(_fileVertexShader);
 m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
 glShaderSource(m_vertexShader, 1, &codeVertexShader, NULL);
 glCompileShader(m_vertexShader);

 int params = -1;
 glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &params);
 if(GL_TRUE!=params)
 {
  fprintf(stderr, "ERROR: GL shader index %i did not compile\n", m_vertexShader);
  m_logger->print_shader_info_log(m_vertexShader);
  return false;
 }
 return true;
}

bool Shader::loadFragmentShader(const char* _fileFragmentShader)
{
 const char* codeFragmentShader = file_read(_fileFragmentShader);
 m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
 glShaderSource(m_fragmentShader, 1, &codeFragmentShader, NULL);
 glCompileShader(m_fragmentShader);

 int params = -1;
 glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &params);
 if(GL_TRUE!=params)
 {
  fprintf(stderr, "ERROR: GL shader index %i did not compile\n", m_fragmentShader);
  m_logger->print_shader_info_log(m_fragmentShader);
  return false;
 }
 return true;
}


bool Shader::createShaderProgramme()
{
    m_shaderProgramme = glCreateProgram();
    glAttachShader(m_shaderProgramme, m_fragmentShader);
    glAttachShader(m_shaderProgramme, m_vertexShader);
    glLinkProgram(m_shaderProgramme);

    int params = -1;
    glGetProgramiv(m_shaderProgramme, GL_LINK_STATUS, &params);
    if(GL_TRUE != params)
    {
     fprintf(stderr, "ERROR: could not link shader programme GL index %u\n", m_shaderProgramme);
     m_logger->print_programme_info_log(m_shaderProgramme);
     return false;
    }
    return true;
}


void Shader::sendModelMatrixToShader(mat4* _modelMatrix)
{
    int modelMatLocation = glGetUniformLocation(m_shaderProgramme, "model");
    glUseProgram(m_shaderProgramme);
    glUniformMatrix4fv(modelMatLocation, 1, GL_FALSE, _modelMatrix->m);
}

void Shader::sendViewMatrixToShader(mat4* _viewMatrix)
{
    int viewMatLocation = glGetUniformLocation(m_shaderProgramme, "view");
    glUseProgram(m_shaderProgramme);
    glUniformMatrix4fv(viewMatLocation, 1, GL_FALSE, _viewMatrix->m);
}

void Shader::sendProjMatrixToShader(mat4* _projMatrix)
{
    int projMatLocation = glGetUniformLocation(m_shaderProgramme, "proj");
    glUseProgram(m_shaderProgramme);
    glUniformMatrix4fv(projMatLocation, 1, GL_FALSE, _projMatrix->m);
}

void Shader::sendCameraRayToShader(vec3 _ray)
{
    glUseProgram(m_shaderProgramme);
    glUniform3fv(2, 1, _ray.v); //To Do: check function
}

void Shader::sendVertexIndexToShader(int _index)
{
    int vertexIndex = glGetUniformLocation(m_shaderProgramme, "vert_index");
    glUseProgram(m_shaderProgramme);
    glUniform1i(vertexIndex, _index);
}


void Shader::sendColourChoiceToShader(vec3 _colour)
{
    int colourLocation = glGetUniformLocation(m_shaderProgramme, "col_choice_vert");
    glUseProgram(m_shaderProgramme);
    glUniform3fv(colourLocation, 1, _colour.v);
}

void Shader::sendColourPickedToShader(vec3 _colour)
{
    int colourLocation = glGetUniformLocation(m_shaderProgramme, "col_picked_vert");
    glUseProgram(m_shaderProgramme);
    glUniform3fv(colourLocation, 1, _colour.v);
}
