#include <iostream>
#include <vector>
#include <cmath>

#include <GL/glew.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

using namespace std;

struct vector3
{
	vector3()
		: x(0.0f), y(0.0f), z(0.0f)
	{

	}

	vector3(float x, float y, float z)
		: x(x), y(y), z(z)
	{

	}

	float x, y, z;
};

struct matrix4
{
	matrix4()
	{
		elements[0] = 0.0f;
		elements[1] = 0.0f;
		elements[2] = 0.0f;
		elements[3] = 0.0f;
		elements[4] = 0.0f;
		elements[5] = 0.0f;
		elements[6] = 0.0f;
		elements[7] = 0.0f;
		elements[8] = 0.0f;
		elements[9] = 0.0f;
		elements[10] = 0.0f;
		elements[11] = 0.0f;
		elements[12] = 0.0f;
		elements[13] = 0.0f;
		elements[14] = 0.0f;
		elements[15] = 0.0f;
	}
	float& operator() (int i, int j)
	{
		return elements[i * 4 + j];
	}

	float operator() (int i, int j) const
	{
		return elements[i * 4 + j];
	}

	float elements[16];
};

matrix4 operator * (const matrix4& m1, const matrix4& m2)
{
	matrix4 res;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			res(i, j) = m1(i, 0) * m2(0, j) + m1(i, 1) * m2(1, j) + m1(i, 2) * m2(2, j) + m1(i, 3) * m2(3, j);
		}
	}

	return res;
}

matrix4 translation(const vector3& position)
{
	matrix4 m;
	m(0, 0) = 1.0f;
	m(1, 1) = 1.0f;
	m(2, 2) = 1.0f;
	m(3, 3) = 1.0f;

	m(3, 0) = position.x;
	m(3, 1) = position.y;
	m(3, 2) = position.z;

	return m;
}

matrix4 uniform_scale(float scale)
{
	matrix4 m;
	m(0, 0) = scale;
	m(1, 1) = scale;
	m(2, 2) = scale;
	m(3, 3) = 1.0f;

	return m;
}

struct vertex
{
	vertex()
	{

	}

	vertex(const vector3& color, const vector3& pos)
		: color(color), pos(pos)
	{

	}

	vector3 color;
	vector3 pos;
};

class mesh
{
public:
	mesh(uint8_t* vertex_data, int vertexCnt, int vertexSize, uint16_t* index_data, int indexCnt, int indexSize)
		: vertex_data(vertex_data), index_data(index_data), indexCnt(indexCnt)
	{
		GLuint bufferNames[2];
		glGenBuffers(2, bufferNames);
		vertexBuffer = *bufferNames;
		indexBuffer = *(bufferNames + 1);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, vertexCnt * vertexSize, vertex_data, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCnt * indexSize, index_data, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	mesh(const mesh& ) = delete;
	mesh& operator=(const mesh& ) = delete;
	mesh(mesh&& ) = delete;
	mesh& operator=(mesh&& ) = delete;

	~mesh()
	{
		GLuint bufferNames[2] = {vertexBuffer, indexBuffer};
		glDeleteBuffers(2, bufferNames);

		delete[] vertex_data;
		delete[] index_data;
	}

	void render() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glInterleavedArrays(GL_C3F_V3F, 0, 0);

		glDrawElements(GL_TRIANGLES, indexCnt, GL_UNSIGNED_SHORT, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

private:
	uint8_t* vertex_data;
	uint16_t* index_data;
	GLuint vertexBuffer;
	GLuint indexBuffer;
	int indexCnt;
};

class model_node
{
public:
	model_node()
	{

	}

	model_node(const model_node& ) = delete;
	model_node& operator=(const model_node& ) = delete;
	model_node(model_node&& ) = delete;
	model_node& operator=(model_node&& ) = delete;

	void render()
	{
		model_node* child = first_child;

		while(child)
		{
			child->render();
			child = child->next_sibling;
		}

		if (m)
		{
			m->render();
		}
	}

	friend model_node* traverse_assimp_scene(const aiNode* curr, const aiScene* scene);

private:
	model_node* next_sibling = nullptr;
	model_node* first_child = nullptr;

	mesh* m = nullptr;
};

class model
{
public:
	model(model_node* root) : root(root)
	{

	}

	model(const model& ) = delete;
	model& operator=(const model& ) = delete;

	model(model&& other)
	{
		root = other.root;
		other.root = nullptr;
	}

	model& operator=(model&& other)
	{
		root = other.root;
		other.root = nullptr;
		return *this;
	}

	~model()
	{
		if (root)
		{
			delete root;
		}
	}

	void render()
	{
		if (root)
		{
			root->render();
		}
	}

private:
	model_node* root = nullptr;
};

mesh* load_mesh_from_assimp_node(const aiNode* node, const aiScene* scene)
{
	int vertex_count = 0;
	int index_count = 0;

	for (const unsigned* iter = node->mMeshes; iter < node->mMeshes + node->mNumMeshes; ++iter)
	{
		vertex_count += scene->mMeshes[*iter]->mNumVertices;
		// assume all faces to be triangles
		index_count += scene->mMeshes[*iter]->mNumFaces * 3;
	}

	if (vertex_count == 0 || index_count == 0)
	{
		return nullptr;
	}

	// not sure how to handle different vertex formats
	// for the moment give it three floats for position
	// plus 3 floats for a color
	int vertex_size = 6 * sizeof(float);
	uint8_t* vertex_data = new uint8_t[vertex_count * vertex_size];
	uint8_t* vertex_data_begin = vertex_data;
	uint16_t* index_data = new uint16_t[index_count];
	uint16_t* index_data_begin = index_data;

	int index_offset = 0;

	for (const unsigned* iter = node->mMeshes; iter < node->mMeshes + node->mNumMeshes; ++iter)
	{
		for (const aiVector3D* iter2 = scene->mMeshes[*iter]->mVertices;
			 iter2 < scene->mMeshes[*iter]->mVertices + scene->mMeshes[*iter]->mNumVertices; ++iter2)
		{
			*reinterpret_cast<float*>(vertex_data) = 1.0f;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = 0.0f;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = 0.0f;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->x;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->y;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->z;
			vertex_data += 4;
		}

		for (const aiFace* iter2 = scene->mMeshes[*iter]->mFaces;
			 iter2 < scene->mMeshes[*iter]->mFaces + scene->mMeshes[*iter]->mNumFaces; ++iter2)
		{
			// again, assume all faces to be triangles
			*index_data = *iter2->mIndices + index_offset;
			++index_data;
			*index_data = *(iter2->mIndices+1) + index_offset;
			++index_data;
			*index_data = *(iter2->mIndices+2) + index_offset;
			++index_data;
		}

		index_offset += scene->mMeshes[*iter]->mNumVertices;
	}

	return new mesh(vertex_data_begin, vertex_count, vertex_size, index_data_begin, index_count, sizeof(uint16_t));
}

model_node* traverse_assimp_scene(const aiNode* curr, const aiScene* scene)
{
	model_node* ret = new model_node;
	ret->m = load_mesh_from_assimp_node(curr, scene);
	model_node** curr_child = &ret->first_child;

	for (aiNode** iter = curr->mChildren;  iter < curr->mChildren + curr->mNumChildren; ++iter)
	{
		*curr_child = traverse_assimp_scene(*iter, scene);
		curr_child = &((*curr_child)->next_sibling);
	}

	return ret;
}

model load_from_assimp_scene(const aiScene* scene)
{
	return traverse_assimp_scene(scene->mRootNode, scene);
}

std::pair<std::vector<vertex>, std::vector<GLushort>> create_circle_mesh_data(int resolution)
{
	std::vector<GLushort> indexData(3*resolution);
	std::vector<vertex> vertexData(resolution + 1);

	vertexData.front() = { vector3(1.0f, 0.0f, 0.0f), vector3(0.0f, 0.0f, -5.0f) };

	float angleStep = (2*M_PI) / (resolution);

	for (int i = 0; i < resolution; ++i)
	{
		float x_coord = cos(i*angleStep);
		float y_coord = sin(i*angleStep);
		vertexData[i+1].pos =  vector3(x_coord, y_coord, -5.0f);
		vertexData[i+1].color = vector3(1.0f, 0.0f, 0.0f);
	}

	for (int i = 0; i < resolution - 1; ++i)
	{
		int pos = i * 3;
		indexData[pos] = 0;
		indexData[pos + 1] = i + 1;
		indexData[pos + 2] = i + 2;
	}

	indexData[(resolution - 1) * 3] = 0;
	indexData[(resolution - 1) * 3 + 1] = resolution;
	indexData[(resolution - 1) * 3 + 2] = 1;

	return std::make_pair(vertexData, indexData);
}

void resize(int width, int height)
{
	float horizontal_view_angle = M_PI * 0.5f;
	float aspect_ratio = float(height) / float(width);
	float near_plane = 1.0f;
	float far_plane = 100.0f;
	float right = near_plane * tanf(horizontal_view_angle * 0.5f);

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-right, right, -right*aspect_ratio, right*aspect_ratio, near_plane, far_plane);
	glMatrixMode(GL_MODELVIEW);
}


int main()
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile("trinity.x",
											 aiProcess_CalcTangentSpace       |
											 aiProcess_Triangulate            |
											 aiProcess_JoinIdenticalVertices  |
											 aiProcess_SortByPType);

	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_Window* window = SDL_CreateWindow("OpenGL", 100, 100, 800, 600, SDL_WINDOW_OPENGL);
	SDL_GLContext glContext = SDL_GL_CreateContext(window);

	glewInit();

	resize(800, 600);

	SDL_Event event;
	bool running = true;

	auto circleData = create_circle_mesh_data(100);

//	mesh circle(circleData.first.data(), circleData.first.size(), sizeof(vertex),
//				circleData.second.data(), circleData.second.size(), sizeof(GLushort));

	model test = load_from_assimp_scene(scene);

	matrix4 mat = translation(vector3(-15.0f, 10.0f, -90.0f));
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -90.0f);
	//glLoadMatrixf(mat.elements);

	while(running)
	{
		if (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		test.render();

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(glContext);
	SDL_Quit();
	return 0;
}
