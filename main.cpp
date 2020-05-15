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

#include "stb_image.h"

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

vector3 linear_interpolation(const vector3& v, const vector3& w, double lerp)
{
	return vector3((1 - lerp) * v.x + lerp * w.x, (1 - lerp) * v.y + lerp * w.y, (1 - lerp) * v.z + lerp * w.z);
}

struct quaternion
{
	quaternion()
	{

	}

	quaternion(float w, float x, float y, float z)
		: w(w), x(x), y(y), z(z)
	{

	}

	float w = 0.0f;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

double quaternion_dot_product(const quaternion& q1, const quaternion& q2)
{
	return q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;
}

quaternion operator * (const quaternion& q1, const quaternion q2)
{
	quaternion prod;
	prod.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
	prod.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
	prod.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
	prod.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
	return prod;
}

quaternion spherical_linear_interpolation(const quaternion& q1, const quaternion& q2, float lerp)
{
	float theta = std::acos(quaternion_dot_product(q1, q2));
	float q1_weight = std::sin(theta * (1 - lerp)) / sin(theta);
	float q2_weight = std::sin(theta * lerp) / sin(theta);

	return quaternion( q1_weight * q1.w + q2_weight * q2.w, q1_weight * q1.x + q2_weight * q2.x,
					   q1_weight * q1.y + q2_weight * q2.y, q1_weight * q1.z + q2_weight * q2.z);
}

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

matrix4 non_uniform_scale(const vector3& v)
{
	matrix4 m;
	m(0, 0) = v.x;
	m(1, 1) = v.y;
	m(2, 2) = v.z;
	m(3, 3) = 1.0f;

	return m;
}

matrix4 rotation(const quaternion& q)
{
	matrix4 ret;

	ret(0, 0) = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	ret(0, 1) = 2.0f * (q.x * q.y - q.w * q.z);
	ret(0, 2) = 2.0f * (q.x * q.z + q.w * q.y);
	ret(0, 3) = 0.0f;
	ret(1, 0) = 2.0f * (q.x * q.y + q.w * q.z);
	ret(1, 1) = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
	ret(1, 2) = 2.0f * (q.y * q.z - q.w * q.x);
	ret(1, 3) = 0.0f;
	ret(2, 0) = 2.0f * (q.x * q.z - q.w * q.y);
	ret(2, 1) = 2.0f * (q.y * q.z + q.w * q.x);
	ret(2, 2) = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	ret(2, 3) = 0.0f;
	ret(3, 0) = 0.0f;
	ret(3, 1) = 0.0f;
	ret(3, 2) = 0.0f;
	ret(3, 3) = 1.0f;

	return ret;
}

matrix4 identity()
{
	matrix4 ret;

	ret(0, 0) = 1.0f;
	ret(0, 1) = 0.0f;
	ret(0, 2) = 0.0f;
	ret(0, 3) = 0.0f;
	ret(1, 0) = 0.0f;
	ret(1, 1) = 1.0f;
	ret(1, 2) = 0.0f;
	ret(1, 3) = 0.0f;
	ret(2, 0) = 0.0f;
	ret(2, 1) = 0.0f;
	ret(2, 2) = 1.0f;
	ret(2, 3) = 0.0f;
	ret(3, 0) = 0.0f;
	ret(3, 1) = 0.0f;
	ret(3, 2) = 0.0f;
	ret(3, 3) = 1.0f;

	return ret;
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

struct bone
{
	matrix4 transform;
	std::string node_ref;
	std::vector<std::pair<int, float>> vertices;
};

class mesh
{
public:
	mesh(uint8_t* vertex_data, int vertexCnt, int vertexSize,
		 uint16_t* index_data, int indexCnt, int indexSize,
		 const std::vector<bone>& bones)
		: vertex_data(vertex_data), index_data(index_data), indexCnt(indexCnt), bones(bones)
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
		glInterleavedArrays(GL_T2F_V3F, 0, 0);

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
	std::vector<bone> bones;
};

class texture
{
public:
	texture(void* pixels, int width, int height)
	{
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	texture(const texture& ) = delete;
	texture& operator=(const texture& ) = delete;
	texture(texture&& ) = delete;
	texture& operator=(texture&& ) = delete;

	~texture()
	{
		glDeleteTextures(1, &tex);
	}

	void set()
	{
		glBindTexture(GL_TEXTURE_2D, tex);
	}

private:
	GLuint tex;
};

class material
{
public:
	~material()
	{
		if (tex != nullptr)
		{
			delete tex;
		}
	}

	void set()
	{
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
		glMaterialfv(GL_FRONT, GL_EMISSION, emissive);
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
		glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
		glMaterialf(GL_FRONT, GL_SHININESS, shininess);
		tex->set();
	}

	float diffuse[4];
	float emissive[4];
	float ambient[4];
	float specular[4];
	float shininess;
	texture* tex = nullptr;
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

	~model_node()
	{
		model_node* child = first_child;

		while(child)
		{
			model_node* tmp = child->next_sibling;
			delete child;
			child = tmp;
		}

		if (m != nullptr)
		{
			delete m;
		}

		if (mat != nullptr)
		{
			delete mat;
		}
	}

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
			mat->set();
			m->render();
		}
	}

	model_node* find(const std::string& name)
	{
		if (name == this->name)
		{
			return this;
		}

		for (model_node* child = first_child; child != nullptr; child = child->next_sibling)
		{
			model_node* ret = child->find(name);

			if (ret != nullptr)
			{
				return ret;
			}
		}

		return nullptr;
	}

	void set_transform_data(const vector3& pos, const quaternion& rot, const vector3& scale)
	{
		this->pos = pos;
		this->rot = rot;
		this->scale = scale;
	}

	void update_transform(const matrix4& parent_mat)
	{
		transform = non_uniform_scale(pos) * rotation(rot) * translation(pos) * parent_mat;

		for (model_node* child = first_child; child != nullptr; child = child->next_sibling)
		{
			child->update_transform(transform);
		}
	}

	friend void load_mesh_from_assimp_node(model_node** mesh_node, const aiNode* assimp_node, const aiScene* scene);
	friend model_node* traverse_assimp_scene(const aiNode* curr, const aiScene* scene);

private:
	model_node* next_sibling = nullptr;
	model_node* first_child = nullptr;

	mesh* m = nullptr;
	material* mat = nullptr;

	matrix4 transform = identity();
	matrix4 original_transform = identity();

	std::string name;

	vector3 pos;
	quaternion rot;
	vector3 scale;
};

struct animation
{
	typedef std::vector<std::pair<double, vector3>> pos_key_list;
	typedef std::vector<std::pair<double, quaternion>> rot_key_list;
	typedef std::vector<std::pair<double, vector3>> scale_key_list;

	std::string node_ref;
	// .x file format and wme both actually use a 32 bit integer for time
	pos_key_list pos_keys;
	rot_key_list rot_keys;
	scale_key_list scale_keys;
	double local_time = 0.0f;

	void update(double delta, double ticks_per_second, model_node* node)
	{
		local_time += delta;

		if (local_time > total_time())
		{
			local_time = 0.0;
		}

		// from wme code
		local_time *= (ticks_per_second / 1000.0);

		vector3 pos = update_positions(local_time);
		quaternion rot = update_rotations(local_time);
		vector3 scale = update_scalings(local_time);

		node->set_transform_data(pos, rot, scale);
	}

	// this is three times the same algorithm, do something generic here
	vector3 update_positions(double local_time)
	{
		if (pos_keys.empty())
		{
			return (vector3(0.0f, 0.0f, 0.0f));
		}

		pos_key_list::iterator key1;
		pos_key_list::iterator key2;

		for (pos_key_list::iterator iter = pos_keys.begin(); iter < pos_keys.end(); ++iter)
		{
			if (iter->first > local_time)
			{
				// wme does something different here, not sure why
				key1 = iter;
				key2 = iter + 1;
			}
		}

		float lerp = (local_time - key1->first) / (key2->first - key1->first);
		return linear_interpolation(key1->second, key2->second, lerp);
	}

	quaternion update_rotations(double local_time)
	{
		if (rot_keys.empty())
		{
			return quaternion(0.0f, 0.0f, 0.0f, 1.0f);
		}

		rot_key_list::iterator key1;
		rot_key_list::iterator key2;

		for (rot_key_list::iterator iter = rot_keys.begin(); iter < rot_keys.end(); ++iter)
		{
			if (iter->first > local_time)
			{
				// wme does something different here, not sure why
				key1 = iter;
				key2 = iter + 1;
			}
		}

		float lerp = (local_time - key1->first) / (key2->first - key1->first);

		quaternion q1 (key1->second.w, -key1->second.x, -key1->second.y, -key1->second.z);
		quaternion q2 (key2->second.w, -key2->second.x, -key2->second.y, -key2->second.z);

		return spherical_linear_interpolation(q1, q2, lerp);
	}

	vector3 update_scalings(double local_time)
	{
		if (scale_keys.size() < 2)
		{
			return vector3(1.0f, 1.0, 1.0f);
		}

		scale_key_list::iterator key1;
		scale_key_list::iterator key2;

		for (scale_key_list::iterator iter = scale_keys.begin(); iter < scale_keys.end(); ++iter)
		{
			if (iter->first > local_time)
			{
				// wme does something different here, not sure why
				key1 = iter;
				key2 = iter + 1;
			}
		}

		float lerp = (local_time - key1->first) / (key2->first - key1->first);
		return linear_interpolation(key1->second, key2->second, lerp);
	}

	double total_time()
	{
		// wme does the same here, appearently the total time
		// is not contained in the .x file format
		double ret = 0.0;

		if (!pos_keys.empty())
		{
			ret = std::max(ret, pos_keys.back().first);
		}

		if (!rot_keys.empty())
		{
			ret = std::max(ret, rot_keys.back().first);
		}

		if (!scale_keys.empty())
		{
			ret = std::max(ret, scale_keys.back().first);
		}

		return ret;
	}
};

typedef std::pair<std::string, std::vector<animation>> animation_set;

class model
{
public:
	model(model_node* root, const std::vector<animation_set>& anim_sets, double ticks_per_second)
		: root(root), animation_sets(anim_sets), ticks_per_second(ticks_per_second)
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

	void update(float delta)
	{
		for (std::vector<animation>::iterator iter = curr_anim->second.begin();
			 iter != curr_anim->second.end(); ++iter)
		{
			model_node* target = find_node(iter->node_ref);

			if (target != nullptr)
			{
				iter->update(delta, ticks_per_second, target);
			}
		}

		root->update_transform(identity());
	}

	void play_anim(const std::string& name)
	{
		for (std::vector<animation_set>::iterator iter = animation_sets.begin(); iter < animation_sets.end(); ++iter)
		{
			if (iter->first == name)
			{
				curr_anim = &(*iter);
			}
		}
	}

	model_node* find_node(const std::string& ref)
	{
		if (root != nullptr)
		{
			return root->find(ref);
		}

		return nullptr;
	}

private:
	model_node* root = nullptr;
	std::vector<animation_set> animation_sets;
	animation_set* curr_anim;
	double ticks_per_second = 4800.0; // x. and wme use an 32-bit integer
};

texture* load_texture(const char* file)
{
	int width = 0;
	int height = 0;
	int compression = 0;

	uint8_t* data = stbi_load(file, &width, &height, &compression, STBI_rgb_alpha);

	if (data == nullptr)
	{
		std::cout << stbi_failure_reason() << std::endl;
		return nullptr;
	}

	uint8_t* data_fixed = new uint8_t[width * height * 4];

	for (int i = 0; i < width * 4; i += 4)
	{
		for (int j = 0; j < height; ++j)
		{
			data_fixed[j * width * 4 + i] = data[(height - j - 1) * width * 4 + i];
			data_fixed[j * width * 4 + i + 1] = data[(height - j - 1) * width * 4 + i + 1];
			data_fixed[j * width * 4 + i + 2] = data[(height - j - 1) * width * 4 + i + 2];
			data_fixed[j * width * 4 + i + 3] = data[(height - j - 1) * width * 4 + i + 3];
		}
	}

	texture* tex = new texture(data_fixed, width, height);
	stbi_image_free(data);
	delete[] data_fixed;
	return tex;
}

void load_mesh_from_assimp_node(model_node** mesh_node, const aiNode* assimp_node, const aiScene* scene)
{
	for (const unsigned* iter = assimp_node->mMeshes; iter < assimp_node->mMeshes + assimp_node->mNumMeshes; ++iter)
	{
		int vertex_count = scene->mMeshes[*iter]->mNumVertices;
		// assume all faces to be triangles
		int index_count = scene->mMeshes[*iter]->mNumFaces * 3;

		if (vertex_count == 0 || index_count == 0)
		{
			continue;
		}

		// not sure how to handle different vertex formats
		// maybe just assume position + texture coords + normals?
		// what does wme do?
		int vertex_size = 5 * sizeof(float);
		uint8_t* vertex_data = new uint8_t[vertex_count * vertex_size];
		uint8_t* vertex_data_begin = vertex_data;
		uint16_t* index_data = new uint16_t[index_count];
		uint16_t* index_data_begin = index_data;

		for (const aiVector3D* iter2 = scene->mMeshes[*iter]->mVertices;
			 iter2 < scene->mMeshes[*iter]->mVertices + vertex_count; ++iter2)
		{
			vertex_data += 8;
			*reinterpret_cast<float*>(vertex_data) = iter2->x;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->y;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->z;
			vertex_data += 4;
		}

		vertex_data = vertex_data_begin;

		for (const aiVector3D* iter2 = scene->mMeshes[*iter]->mTextureCoords[0];
			 iter2 < scene->mMeshes[*iter]->mTextureCoords[0] + vertex_count; ++iter2)
		{
			*reinterpret_cast<float*>(vertex_data) = iter2->x;
			vertex_data += 4;
			*reinterpret_cast<float*>(vertex_data) = iter2->y;
			vertex_data += 16;
		}

		for (const aiFace* iter2 = scene->mMeshes[*iter]->mFaces;
			 iter2 < scene->mMeshes[*iter]->mFaces + scene->mMeshes[*iter]->mNumFaces; ++iter2)
		{
			// again, assume all faces to be triangles
			*index_data = *iter2->mIndices;
			++index_data;
			*index_data = *(iter2->mIndices+1);
			++index_data;
			*index_data = *(iter2->mIndices+2);
			++index_data;
		}

		aiMaterial* assimp_mat = scene->mMaterials[scene->mMeshes[*iter]->mMaterialIndex];

		aiColor3D diffuse;
		assimp_mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		aiColor3D emissive;
		assimp_mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
		aiColor3D ambient;
		assimp_mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
		aiColor3D specular;
		assimp_mat->Get(AI_MATKEY_COLOR_SPECULAR, specular);

		material* mat = new material;
		mat->diffuse[0] = diffuse.r;
		mat->diffuse[1] = diffuse.g;
		mat->diffuse[2] = diffuse.b;
		mat->diffuse[3] = 1.0f;

		mat->emissive[0] = emissive.r;
		mat->emissive[1] = emissive.g;
		mat->emissive[2] = emissive.b;
		mat->emissive[3] = 1.0f;

		mat->ambient[0] = ambient.r;
		mat->ambient[1] = ambient.g;
		mat->ambient[2] = ambient.b;
		mat->ambient[3] = 1.0f;

		mat->specular[0] = specular.r;
		mat->specular[1] = specular.g;
		mat->specular[2] = specular.b;
		mat->specular[3] = 1.0f;

		assimp_mat->Get(AI_MATKEY_SHININESS, mat->shininess);

		for (int i = 0 ; i < assimp_mat->GetTextureCount(aiTextureType_DIFFUSE); ++i)
		{
			aiString texture_path;
			assimp_mat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, i), texture_path);
			mat->tex = load_texture(texture_path.C_Str());
		}

		std::vector<bone> bones;

		for (auto iter2 = scene->mMeshes[*iter]->mBones; iter2 < scene->mMeshes[*iter]->mBones + scene->mMeshes[*iter]->mNumBones; ++iter2)
		{
			bones.push_back(bone());
			bones.back().node_ref = (*iter2)->mName.C_Str();

			for (auto iter3 = (*iter2)->mWeights; iter3 < (*iter2)->mWeights + (*iter2)->mNumWeights; ++iter3)
			{
				bones.back().vertices.push_back(std::make_pair(iter3->mVertexId, iter3->mWeight));
			}

			bones.back().transform.operator()(0, 0) = (*iter2)->mOffsetMatrix.a1;
			bones.back().transform.operator()(0, 1) = (*iter2)->mOffsetMatrix.a2;
			bones.back().transform.operator()(0, 2) = (*iter2)->mOffsetMatrix.a3;
			bones.back().transform.operator()(0, 3) = (*iter2)->mOffsetMatrix.a4;
			bones.back().transform.operator()(1, 0) = (*iter2)->mOffsetMatrix.b1;
			bones.back().transform.operator()(1, 1) = (*iter2)->mOffsetMatrix.b2;
			bones.back().transform.operator()(1, 2) = (*iter2)->mOffsetMatrix.b3;
			bones.back().transform.operator()(1, 3) = (*iter2)->mOffsetMatrix.b4;
			bones.back().transform.operator()(2, 0) = (*iter2)->mOffsetMatrix.c1;
			bones.back().transform.operator()(2, 1) = (*iter2)->mOffsetMatrix.c2;
			bones.back().transform.operator()(2, 2) = (*iter2)->mOffsetMatrix.c3;
			bones.back().transform.operator()(2, 3) = (*iter2)->mOffsetMatrix.c4;
			bones.back().transform.operator()(3, 0) = (*iter2)->mOffsetMatrix.d1;
			bones.back().transform.operator()(3, 1) = (*iter2)->mOffsetMatrix.d2;
			bones.back().transform.operator()(3, 2) = (*iter2)->mOffsetMatrix.d3;
			bones.back().transform.operator()(3, 3) = (*iter2)->mOffsetMatrix.d4;
		}

		*mesh_node = new model_node;
		(*mesh_node)->m = new mesh(vertex_data_begin, vertex_count, vertex_size,
								   index_data_begin, index_count, sizeof(uint16_t),
								   bones);
		(*mesh_node)->mat = mat;
		mesh_node = &(*mesh_node)->next_sibling;
	}
}

model_node* traverse_assimp_scene(const aiNode* curr, const aiScene* scene)
{
	model_node* ret = new model_node;
	model_node** curr_child = &ret->first_child;

	for (aiNode** iter = curr->mChildren;  iter < curr->mChildren + curr->mNumChildren; ++iter)
	{
		*curr_child = traverse_assimp_scene(*iter, scene);
		curr_child = &((*curr_child)->next_sibling);
	}

	load_mesh_from_assimp_node(curr_child, curr, scene);
	ret->name = curr->mName.C_Str();

	ret->transform(0, 0) = curr->mTransformation.a1;
	ret->transform(0, 1) = curr->mTransformation.a2;
	ret->transform(0, 2) = curr->mTransformation.a3;
	ret->transform(0, 3) = curr->mTransformation.a4;
	ret->transform(1, 0) = curr->mTransformation.b1;
	ret->transform(1, 1) = curr->mTransformation.b2;
	ret->transform(1, 2) = curr->mTransformation.b3;
	ret->transform(1, 3) = curr->mTransformation.b4;
	ret->transform(2, 0) = curr->mTransformation.c1;
	ret->transform(2, 1) = curr->mTransformation.c2;
	ret->transform(2, 2) = curr->mTransformation.c3;
	ret->transform(2, 3) = curr->mTransformation.c4;
	ret->transform(3, 0) = curr->mTransformation.d1;
	ret->transform(3, 1) = curr->mTransformation.d2;
	ret->transform(3, 2) = curr->mTransformation.d3;
	ret->transform(3, 3) = curr->mTransformation.d4;

	return ret;
}

model* load_from_assimp_scene(const aiScene* scene)
{
	model_node* root = traverse_assimp_scene(scene->mRootNode, scene);
	std::vector<animation_set> anim_sets;

	double ticks_per_second = 0.0;

	for (aiAnimation** iter = scene->mAnimations; iter < scene->mAnimations + scene->mNumAnimations; ++iter)
	{
		anim_sets.resize(anim_sets.size() + 1);
		anim_sets.back().first = std::string((*iter)->mName.C_Str());
		ticks_per_second = (*iter)->mTicksPerSecond;

		for (aiNodeAnim** iter2 = (*iter)->mChannels; iter2 < (*iter)->mChannels + (*iter)->mNumChannels; ++iter2)
		{
			anim_sets.back().second.push_back(animation());
			anim_sets.back().second.back().node_ref = std::string((*iter2)->mNodeName.C_Str());

			for (aiVectorKey* iter3 = (*iter2)->mPositionKeys; iter3 < (*iter2)->mPositionKeys + (*iter2)->mNumPositionKeys; ++iter3)
			{
				anim_sets.back().second.back().pos_keys.resize(anim_sets.back().second.back().pos_keys.size() + 1);
				anim_sets.back().second.back().pos_keys.back().first = iter3->mTime;
				anim_sets.back().second.back().pos_keys.back().second.x = iter3->mValue.x;
				anim_sets.back().second.back().pos_keys.back().second.y = iter3->mValue.y;
				anim_sets.back().second.back().pos_keys.back().second.z = iter3->mValue.z;
			}

			for (aiVectorKey* iter3 = (*iter2)->mScalingKeys; iter3 < (*iter2)->mScalingKeys + (*iter2)->mNumScalingKeys; ++iter3)
			{
				anim_sets.back().second.back().scale_keys.resize(anim_sets.back().second.back().scale_keys.size() + 1);
				anim_sets.back().second.back().scale_keys.back().first = iter3->mTime;
				anim_sets.back().second.back().scale_keys.back().second.x = iter3->mValue.x;
				anim_sets.back().second.back().scale_keys.back().second.y = iter3->mValue.y;
				anim_sets.back().second.back().scale_keys.back().second.z = iter3->mValue.z;
			}

			for (aiQuatKey* iter3 = (*iter2)->mRotationKeys; iter3 < (*iter2)->mRotationKeys + (*iter2)->mNumRotationKeys; ++iter3)
			{
				anim_sets.back().second.back().rot_keys.resize(anim_sets.back().second.back().rot_keys.size() + 1);
				anim_sets.back().second.back().rot_keys.back().first = iter3->mTime;
				anim_sets.back().second.back().rot_keys.back().second.w = iter3->mValue.w;
				anim_sets.back().second.back().rot_keys.back().second.x = iter3->mValue.x;
				anim_sets.back().second.back().rot_keys.back().second.y = iter3->mValue.y;
				anim_sets.back().second.back().rot_keys.back().second.z = iter3->mValue.z;
			}
		}
	}

	return new model(root, anim_sets, ticks_per_second);
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

	model* test = load_from_assimp_scene(scene);

	matrix4 mat = translation(vector3(-15.0f, 10.0f, -90.0f));
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -90.0f);
	//glLoadMatrixf(mat.elements);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	float delta = 0.0f;

	test->play_anim("walk");

	while(running)
	{
		uint32_t time_elapsed_begin = SDL_GetTicks();

		if (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		glClearColor(0.2f, 0.4f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		test->render();

		SDL_GL_SwapWindow(window);

		test->update(delta);

		uint32_t time_elapsed_end = SDL_GetTicks();
		delta = static_cast<float>(time_elapsed_begin - time_elapsed_end) * 1000.0f;
	}

	delete test;

	SDL_GL_DeleteContext(glContext);
	SDL_Quit();
	return 0;
}
