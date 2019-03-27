// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <iostream>
// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>
#include <common/objloader.hpp>

static GLubyte *pixels = NULL;
static GLfloat *depth_pixels = NULL;
static const GLenum FORMAT = GL_RGBA;
static const GLuint FORMAT_NBYTES = 4;
static const unsigned int HEIGHT = 1080;
static const unsigned int WIDTH = 1920;
static unsigned int nscreenshots = 0;


static void create_ppm(char *prefix, int frame_id, unsigned int width, unsigned int height,
	unsigned int color_max, unsigned int pixel_nbytes, GLubyte *pixels) {
	size_t i, j, k, cur;
	enum Constants { max_filename = 256 };
	char filename[max_filename];
	snprintf(filename, max_filename, "%s%d.ppm", prefix, frame_id);
	FILE *f = fopen(filename, "w");
	fprintf(f, "P3\n%d %d\n%d\n", width, HEIGHT, 255);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			cur = pixel_nbytes * ((height - i - 1) * width + j);
			fprintf(f, "%3d %3d %3d ", pixels[cur], pixels[cur + 1], pixels[cur + 2]);
		}
		fprintf(f, "\n");
	}
	fclose(f);
}

static void create_bin(char *prefix, int frame_id, unsigned int width, unsigned int height,
	unsigned int color_max, unsigned int pixel_nbytes, GLubyte *pixels) {
	size_t i, j, k, cur;
	enum Constants { max_filename = 256 };
	char filename[max_filename];
	snprintf(filename, max_filename, "%s%d.bin", prefix, frame_id);
	FILE *f = fopen(filename, "wb");
	fwrite(&height, sizeof(height), 1, f);
	fwrite(&width, sizeof(width), 1, f);
	fwrite(&pixel_nbytes, sizeof(pixel_nbytes), 1, f);
	fwrite(pixels, sizeof(pixels[0]), width * height * pixel_nbytes, f);
	fclose(f);
}static void create_depth_bin(char *prefix, int frame_id, unsigned int width, unsigned int height, GLfloat *pixels) {
	size_t i, j, k, cur;
	enum Constants { max_filename = 256 };
	char filename[max_filename];
	snprintf(filename, max_filename, "%sdepth%d.bin", prefix, frame_id);
	FILE *f = fopen(filename, "wb");
	fwrite(&height, sizeof(height), 1, f);
	fwrite(&width, sizeof(width), 1, f);
	fwrite(pixels, sizeof(pixels[0]), width * height, f);
	fclose(f);
}
int main(int argc, char* argv[])
{
    std::string arg0 = argv[0];
    std::size_t found = arg0.find_last_of("/\\");
    std::string path = arg0.substr(0,found+1);
    std::cout<<path<<std::endl;
	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(WIDTH, HEIGHT, "Rayshaper Windows Player", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Hide the mouse and enable unlimited mouvement
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Set the mouse at the center of the screen
    glfwPollEvents();
    glfwSetCursorPos(window, WIDTH /2, HEIGHT/2);

	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	// Cull triangles which normal is not towards the camera
	//glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders( "TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader" );

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    glfwSetCursorPos(window, WIDTH /2, HEIGHT/2);
	int i = 1;
	int frames = argc > 1 ? atoi(argv[1]) : 2147483647;
	double lasttime = glfwGetTime();
    bool pause = false;
	GLuint vertexbuffer;


	GLuint uvbuffer;
	
	pixels = new GLubyte[FORMAT_NBYTES * WIDTH * HEIGHT];
    depth_pixels = new GLfloat[WIDTH*HEIGHT];
	std::vector<GLuint> Texture;
	std::vector<std::vector<glm::vec3> >vertices;
	std::vector<std::vector<glm::vec2> > uvs;
	std::vector<std::vector<glm::vec3> > normals;

	glm::mat4 ViewMatrix;
	glm::mat4 ProjectionMatrix;
	glm::vec3 position = glm::vec3(0, 0, 0);
	float FoV = 60;
// Projection matrix : 45?Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	ProjectionMatrix = glm::perspective(glm::radians(FoV), 16.0f / 9.0f, 0.1f, 100.0f);
	// Camera matrix
	ViewMatrix = glm::lookAt(
		position,           // Camera is here
		position + glm::vec3(0,0,-1), // and looks here : at the same position, plus "direction"
		glm::vec3(0,1,0)                  // Head is up (set to 0,-1,0 to look upside-down)
	);

	for (int i = 0; i < frames; i++) {
		char tex_filename[50];
		sprintf(tex_filename, "%s%06d.bmp", path.c_str(), i%frames + 1);
		GLuint t_texture = loadBMP_custom(tex_filename);
		if (!t_texture) {
			frames = i;
			break;
		}
		char obj_filename[50];
		sprintf(obj_filename, "%s%06d.obj", path.c_str(), i%frames + 1);
		std::vector<glm::vec3> t_vertices;
		std::vector<glm::vec2> t_uvs;
		std::vector<glm::vec3> t_normals;
		bool res = loadOBJ(obj_filename, t_vertices, t_uvs, t_normals);
		if (!res) {
			frames = i;
			break;
		}
		Texture.push_back(t_texture);
		vertices.push_back(t_vertices);
		uvs.push_back(t_uvs);
		normals.push_back(t_normals);
	}
	if (frames == 0)
		return 0;
	bool record_mode = false;
	float last_R_press = 0;
	bool record_Bmode = false;
	float last_B_press = 0;
	glfwSetCursorPos(window, WIDTH / 2, HEIGHT / 2);
	do {
	// Load the texture
//	GLuint Texture = loadDDS("uvmap.DDS");

	
	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

	// Read our .obj file
 // Won't be used at the moment.

	// Load it into a VBO

	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices[i%frames].size() * sizeof(glm::vec3), &vertices[i%frames][0], GL_STATIC_DRAW);

	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, uvs[i%frames].size() * sizeof(glm::vec2), &uvs[i%frames][0], GL_STATIC_DRAW);
	
	

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programID);

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		
		glm::mat4 ViewMatrix = getViewMatrix();
		//int speed_factor = 100;
		//float step = 2 * 3.1415 / speed_factor;
		//float x_scale = 0.2;
		//float y_scal2 = 0.2;
		//float x_offset = 0;
		//float y_offset = 0;
		//float z_offset = 0;
		//ViewMatrix = glm::lookAt(
		//	glm::vec3(x_scale * cos((i % speed_factor)*step) + x_offset, 
		//				y_scal2 * sin((i % speed_factor)*step) + y_offset, 
		//				0 + z_offset),// Camera is here
		//	glm::vec3(x_scale * cos((i % speed_factor)*step) + x_offset, 
		//				y_scal2 * sin((i % speed_factor)*step) + y_offset,
		//				0 + z_offset) + glm::vec3(0, 0, -1), // and looks here : at the same position, plus "direction"
		//	glm::vec3(0, 1, 0)                  // Head is up (set to 0,-1,0 to look upside-down)
		//);
		glm::mat4 ModelMatrix = glm::mat4(1);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture[i%frames]);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(TextureID, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// Draw the triangle !
		glDrawArrays(GL_TRIANGLES, 0, vertices[i%frames].size() );

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
            pause = true;
        }
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) {
            pause = false;
        }
		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
			if (glfwGetTime() - last_R_press > 5) {
				record_mode = !record_mode;
				last_R_press = glfwGetTime();
			}
		}
		if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
			if (glfwGetTime() - last_B_press > 1) {
				record_Bmode = !record_Bmode;
				last_B_press = glfwGetTime();
			}
		}
		if (glfwGetTime() - lasttime > 0.03) {
			lasttime = glfwGetTime();
            if (!pause)
                i++;
			glReadPixels(0, 0, WIDTH, HEIGHT, FORMAT, GL_UNSIGNED_BYTE, pixels);
			if (record_mode) {
				create_ppm("tmp", nscreenshots, WIDTH, HEIGHT, 255, FORMAT_NBYTES, pixels);
				nscreenshots++;
			}
			else if (record_Bmode) {
				create_bin("tmp", nscreenshots, WIDTH, HEIGHT, 255, FORMAT_NBYTES, pixels);
				nscreenshots++;
			}
		}
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            glReadPixels(0, 0, WIDTH, HEIGHT, GL_DEPTH_COMPONENT, GL_FLOAT, depth_pixels);
            create_depth_bin("tmp", nscreenshots, WIDTH, HEIGHT, depth_pixels);
        }
		glDeleteBuffers(1, &vertexbuffer);
		glDeleteBuffers(1, &uvbuffer);
	} // Check if the ESC key was pressed or the window was closed
	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 );

	delete [] pixels;
    delete [] depth_pixels;
	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteProgram(programID);
	for(int i = 0; i < frames; i++)
		glDeleteTextures(1, &Texture[i]);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

