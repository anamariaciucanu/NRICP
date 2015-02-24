/*
Compile with  g++ -o demo main.cpp libGLEW.a libglfw3.a -I include -lGL -lX11 -lXxf86vm -lXrandr -lpthread -lXi -lm -lXinerama -lXcursor
Use valgrind --leak-check=full to check for memory leaks.
*/

//Resources used:
//http://openglbook.com/
//Anton's OpenGL Tutorials Book
//www.opengl-tutorial.org



//TO DO: Memory release
//TO DO: Destroy buffers
//TO DO: Create vector of meshes in glfwcontainer.cpp
//TO DO: Calculate vertex normals from face normals

#include "glfwcontainer.h"


GLFWContainer* glfw_container = new GLFWContainer(1280, 720);


void calculateClickRay(double _mouseX, double _mouseY)
{
    //2D Viewport Coordinates -> 3D Normalised Device Coordinates
    float x = (2.0f * _mouseX)/glfw_container->getWidth() - 1.0f;
    float y = 1.0f - (2.0f * _mouseY)/glfw_container->getHeight();
    float z = 1.0f;
    vec3 ray_nds = vec3(x, y, z);
    //-> Homogeneous Clip Coordinates
    vec4 ray_clip = vec4(ray_nds.v[0], ray_nds.v[1], -1.0, 1.0);
    //No need to reverse perspective division here
    //4D Eye (Camera) Coordinates
    vec4 ray_eye = inverse(glfw_container->getProjMatrix())*ray_clip;
    ray_eye = vec4(ray_eye.v[0], ray_eye.v[1], -1.0, 0.0);
    //4D World Coordinates
    vec4 aux_wor= inverse(glfw_container->getViewMatrix())*ray_eye;
    vec3 ray_wor = vec3(aux_wor.v[0], aux_wor.v[1], aux_wor.v[2]);
    ray_wor = normalise(ray_wor);
    Camera* cam = glfw_container->getCamera();
    NRICP* nrICP = glfw_container->getNRICP();
    Vector3f camera(cam->x(), cam->y(), cam->z());
    Vector3f ray = Vector3f(ray_wor.v[0], ray_wor.v[1], ray_wor.v[2]);
    ray = ray/sqrt(ray[0]*ray[0] + ray[1]*ray[1] + ray[2]*ray[2]);

    int intersection = nrICP->getTemplate()->whereIsIntersectingMesh(true, -1, camera, ray);

    if(intersection >= 0)
    {
        nrICP->setTemplateAuxIndex(intersection);
    }
}


void glfw_window_size_callback(GLFWwindow* _window, int _width, int _height)
{
    glfw_container->setWidth(_width);
    glfw_container->setHeight(_height);
}

void glfw_error_callback(int error, const char* description)
{
   glfw_container->getLogger()->gl_log_err("GLFW ERROR: code %i msg: %s\n", error, description);
}

void mouseClickEvent(GLFWwindow *_window, int _button, int _action, int _mods)
{

  if(_button == GLFW_MOUSE_BUTTON_1 && _action == GLFW_PRESS)
  {
   double xpos;
   double ypos;
   glfwGetCursorPos(_window, &xpos, &ypos);
   calculateClickRay(xpos, ypos);
  }
}


int main(){
    omp_set_num_threads(8);
    Eigen::setNbThreads(8);
    Eigen::initParallel();

    glfw_container->initializeWindow();
    glfwSetWindowSizeCallback(glfw_container->getWindow(), glfw_window_size_callback);
    glfwSetMouseButtonCallback(glfw_container->getWindow(), mouseClickEvent);
    glfwSetErrorCallback(glfw_error_callback);
    glfw_container->initializeDrawing();
    glfw_container->loopDrawing();

	return 0;
}


