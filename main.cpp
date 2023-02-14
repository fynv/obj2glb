#include <string>
#include <vector>
#include <unordered_map>
#include <glm.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include "crc64.h"

inline bool exists_test(const char* name)
{
	if (FILE* file = fopen(name, "r"))
	{
		fclose(file);
		return true;
	}
	else
	{
		return false;
	}
}


int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("obj2glb input.obj output.glb\n");
		return 0;
	}

	std::string path_model = std::filesystem::path(argv[1]).parent_path().u8string()+"/";	

	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      err;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &err, argv[1], path_model.c_str());

	struct Img
	{
		std::string name;
		int width, height, chn;
		uint8_t* data = nullptr;

		void Load(std::string filename, const std::string& path)
		{
			if (!exists_test(filename.c_str()))
			{
				filename = path + filename;
			}			
			data = stbi_load(filename.c_str(), &width, &height, &chn, 3);
		}
	};

	std::vector<Img> textures_in;

	struct MaterialIn
	{
		std::string name;
		glm::vec3 color_diffuse = { 0.0f, 0.0f, 0.0f };
		int idx_diffuse = -1;
		glm::vec3 color_specular = { 0.0f, 0.0f, 0.0f };
		int idx_specular = -1;
		glm::vec3 color_emission = { 0.0f, 0.0f, 0.0f };		
		int idx_emission = -1;
		float shininess = 0.0f;		
		int idx_alpha = -1;
		int idx_normal = -1;
	};

	int num_materials = (int)materials.size();
	std::vector<MaterialIn> materials_in(num_materials);

	for (int i = 0; i < num_materials; i++)
	{
		tinyobj::material_t& material = materials[i];
		MaterialIn& material_in = materials_in[i];
		std::string name_material = material.name;
		material_in.name = name_material;

		{
			material_in.color_diffuse = { material.diffuse[0], material.diffuse[1], material.diffuse[2] };
			std::string filename = materials[i].diffuse_texname;

			if (filename != "")
			{
				char fn_tex[1024];
				_splitpath(filename.c_str(), nullptr, nullptr, fn_tex, nullptr);
				int idx_tex = (int)textures_in.size();
				Img img;
				img.name = fn_tex;
				img.Load(filename, path_model);
				textures_in.push_back(img);
				material_in.idx_diffuse = idx_tex;
			}
		}

		{
			material_in.color_specular = { material.specular[0], material.specular[1], material.specular[2] };
			std::string filename = materials[i].specular_texname;

			if (filename != "")
			{
				char fn_tex[1024];
				_splitpath(filename.c_str(), nullptr, nullptr, fn_tex, nullptr);
				int idx_tex = (int)textures_in.size();
				Img img;
				img.name = fn_tex;
				img.Load(filename, path_model);
				textures_in.push_back(img);
				material_in.idx_specular = idx_tex;
			}
		}

		{	
			material_in.color_emission = { material.emission[0], material.emission[1], material.emission[2] };
			std::string filename = materials[i].emissive_texname;

			if (filename != "")
			{
				char fn_tex[1024];
				_splitpath(filename.c_str(), nullptr, nullptr, fn_tex, nullptr);
				int idx_tex = (int)textures_in.size();
				Img img;
				img.name = fn_tex;
				img.Load(filename, path_model);
				textures_in.push_back(img);
				material_in.idx_emission = idx_tex;
			}
		}
		material_in.shininess = material.shininess;
		
		{
			std::string filename = materials[i].alpha_texname;
			if (filename != "")
			{
				char fn_tex[1024];
				_splitpath(filename.c_str(), nullptr, nullptr, fn_tex, nullptr);
				int idx_tex = (int)textures_in.size();
				Img img;
				img.name = fn_tex;
				img.Load(filename, path_model);
				textures_in.push_back(img);
				material_in.idx_alpha = idx_tex;
			}
		}

		{
			std::string filename = materials[i].normal_texname;
			if (filename != "")
			{
				char fn_tex[1024];
				_splitpath(filename.c_str(), nullptr, nullptr, fn_tex, nullptr);
				int idx_tex = (int)textures_in.size();
				Img img;
				img.name = fn_tex;
				img.Load(filename, path_model);
				textures_in.push_back(img);
				material_in.idx_normal = idx_tex;
			}
		}	
	}

	struct Image
	{
		std::string name;
		std::string mimeType;
		int width, height;
		std::vector<unsigned char> data;
		std::vector<unsigned char> storage;
	};

	std::vector<Image> textures;

	struct Material
	{
		std::string name;
		bool blending = false;
		float metallicFactor;
		float roughnessFactor;
		glm::vec4 baseColorFactor;
		int baseColorTex = -1;
		glm::vec3 emissionFactor;
		float strength_emission = 1.0f;
		int emissiveTex = -1;
		int normalTex = -1;
	};

	std::vector<Material> materials_mid(num_materials);
	for (int i = 0; i < num_materials; i++)
	{
		MaterialIn& material_in = materials_in[i];
		Material& material_mid = materials_mid[i];
		material_mid.name = material_in.name;

		glm::vec3 color_diffuse = material_in.color_diffuse;
		
		glm::vec3 color_specular = material_in.color_specular;
		float iDiffuse = glm::length(color_diffuse);
		float iSpecular = glm::length(color_specular);
		if (iSpecular + iDiffuse > 0.0f)
		{
			material_mid.metallicFactor = iSpecular / (iSpecular + iDiffuse);
		}

		glm::vec3 base_color = color_diffuse + color_specular;
		material_mid.baseColorFactor = { glm::clamp(base_color, 0.0f, 1.0f), 1.0f };

		float strength_emission = 1.0f;
		if (material_in.color_emission.x > strength_emission) strength_emission = material_in.color_emission.x;
		if (material_in.color_emission.y > strength_emission) strength_emission = material_in.color_emission.y;
		if (material_in.color_emission.z > strength_emission) strength_emission = material_in.color_emission.z;

		material_mid.emissionFactor = material_in.color_emission/ strength_emission;
		material_mid.strength_emission = strength_emission;
		
		float shininess = material_in.shininess;
		float r4 = 3.0f / (powf(2.0f, (shininess + 3.0f) * 0.5f) - 1.0f);
		float r = sqrtf(sqrtf(r4));
		if (r < 0.0f) r = 0.0f;
		if (r > 1.0f) r = 1.0f;
		material_mid.roughnessFactor = r;

		if (material_in.idx_diffuse >= 0)
		{			
			int idx_tex = (int)textures.size();
			material_mid.baseColorTex = idx_tex;

			Img& img_in = textures_in[material_in.idx_diffuse];
			if (material_in.idx_alpha >= 0)
			{
				material_mid.blending = true;

				Img& alpha_in = textures_in[material_in.idx_alpha];

				Image img_out;
				img_out.name = img_in.name;
				img_out.width = img_in.width;
				img_out.height = img_in.height;
				img_out.mimeType = "image/png";
				img_out.data.resize((size_t)img_out.width* (size_t)img_out.height * 4);

				for (size_t pix_idx = 0; pix_idx < (size_t)img_out.width * (size_t)img_out.height; pix_idx++)
				{
					uint8_t* p_out = img_out.data.data() + pix_idx * 4;
					const uint8_t* p_img = img_in.data + pix_idx * 3;
					const uint8_t* p_alpha = alpha_in.data + pix_idx * 3;
					p_out[0] = p_img[0]; p_out[1] = p_img[1]; p_out[2] = p_img[2];
					p_out[3] = p_alpha[0];
				}

				textures.push_back(std::move(img_out));
			}
			else
			{
				Image img_out;
				img_out.name = img_in.name;
				img_out.width = img_in.width;
				img_out.height = img_in.height;
				img_out.mimeType = "image/jpeg";
				img_out.data.resize((size_t)img_out.width * (size_t)img_out.height * 4);

				for (size_t pix_idx = 0; pix_idx < (size_t)img_out.width * (size_t)img_out.height; pix_idx++)
				{
					uint8_t* p_out = img_out.data.data() + pix_idx * 4;
					const uint8_t* p_img = img_in.data + pix_idx * 3;
					p_out[0] = p_img[0]; p_out[1] = p_img[1]; p_out[2] = p_img[2];
					p_out[3] = 255;
				}

				textures.push_back(std::move(img_out));
			}
		}
		else if (material_in.idx_alpha >= 0)
		{
			int idx_tex = (int)textures.size();
			material_mid.baseColorTex = idx_tex;

			material_mid.blending = true;
			Img& alpha_in = textures_in[material_in.idx_alpha];

			Image img_out;
			img_out.name = alpha_in.name;
			img_out.width = alpha_in.width;
			img_out.height = alpha_in.height;
			img_out.mimeType = "image/png";
			img_out.data.resize((size_t)img_out.width * (size_t)img_out.height * 4);

			for (size_t pix_idx = 0; pix_idx < (size_t)img_out.width * (size_t)img_out.height; pix_idx++)
			{
				uint8_t* p_out = img_out.data.data() + pix_idx * 4;
				const uint8_t* p_alpha = alpha_in.data + pix_idx * 3;
				p_out[0] = 255; p_out[1] = 255; p_out[2] = 255;
				p_out[3] = p_alpha[0];
			}

			textures.push_back(std::move(img_out));
		}

		if (material_in.idx_emission >= 0)
		{
			int idx_tex = (int)textures.size();
			material_mid.emissiveTex = idx_tex;

			Img& img_in = textures_in[material_in.idx_emission];

			Image img_out;
			img_out.name = img_in.name;
			img_out.width = img_in.width;
			img_out.height = img_in.height;
			img_out.mimeType = "image/jpeg";
			img_out.data.resize((size_t)img_out.width * (size_t)img_out.height * 4);

			for (size_t pix_idx = 0; pix_idx < (size_t)img_out.width * (size_t)img_out.height; pix_idx++)
			{
				uint8_t* p_out = img_out.data.data() + pix_idx * 4;
				const uint8_t* p_img = img_in.data + pix_idx * 3;
				p_out[0] = p_img[0]; p_out[1] = p_img[1]; p_out[2] = p_img[2];
				p_out[3] = 255;
			}

			textures.push_back(std::move(img_out));
			
		}

		if (material_in.idx_normal >= 0)
		{
			int idx_tex = (int)textures.size();
			material_mid.normalTex = idx_tex;

			Img& img_in = textures_in[material_in.idx_normal];

			Image img_out;
			img_out.name = img_in.name;
			img_out.width = img_in.width;
			img_out.height = img_in.height;
			img_out.mimeType = "image/jpeg";
			img_out.data.resize((size_t)img_out.width * (size_t)img_out.height * 4);

			for (size_t pix_idx = 0; pix_idx < (size_t)img_out.width * (size_t)img_out.height; pix_idx++)
			{
				uint8_t* p_out = img_out.data.data() + pix_idx * 4;
				const uint8_t* p_img = img_in.data + pix_idx * 3;
				p_out[0] = p_img[0]; p_out[1] = p_img[1]; p_out[2] = p_img[2];
				p_out[3] = 255;
			}
			textures.push_back(std::move(img_out));
		}
	}

	for (size_t i = 0; i < textures_in.size(); i++)
	{
		stbi_image_free(textures_in[i].data);
	}

	struct Primitive
	{
		int material;
		std::vector<glm::ivec3> indices;
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec3> colors;
		std::vector<glm::vec2> texcoords;
	};

	struct Mesh
	{		
		std::string name;
		std::vector<Primitive> primitives;
	};

	int num_meshes = (int)shapes.size();
	std::vector<Mesh> meshes(num_meshes);

	for (int i = 0; i < num_meshes; i++)
	{		
		tinyobj::shape_t& shape = shapes[i];
		Mesh& mesh_out = meshes[i];
		mesh_out.name = shape.name;

		std::vector<Primitive>& primitives = mesh_out.primitives;
		std::unordered_map<int, int> material_prim_map;
				
		for (size_t j = 0; j < shape.mesh.material_ids.size(); j++)
		{
			int material_id = shape.mesh.material_ids[j];
			auto iter = material_prim_map.find(material_id);
			if (iter == material_prim_map.end())
			{
				int prim_id = (int)primitives.size();
				primitives.resize(prim_id + 1);
				Primitive& prim_out = primitives[prim_id];
				prim_out.material = material_id;
				material_prim_map[material_id] = prim_id;
			}
		}
		
		for (int i_prim = 0; i_prim < (int)primitives.size(); i_prim++)
		{
			Primitive& prim_out = primitives[i_prim];
			int i_material = prim_out.material;			
			std::unordered_map<uint64_t, size_t> ind_map;

			for (int j = 0; j < (int)shape.mesh.material_ids.size(); j++)
			{				
				if (shape.mesh.material_ids[j] != i_material) continue;
				
				glm::ivec3 cur_ind;
				for (int k = 0; k < 3; k++)
				{
					int i_vertex = j * 3 + k;
					tinyobj::index_t& index = shape.mesh.indices[i_vertex];

					struct Attributes
					{
						glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
						glm::vec3 norm = { 0.0f, 0.0f, 0.0f };
						glm::vec3 color = { 0.0f, 0.0f, 0.0f };
						glm::vec2 uv = { 0.0f, 0.0f };
					};

					Attributes att;

					float* vp = &attrib.vertices[3 * index.vertex_index];
					att.pos = glm::vec3(vp[0], vp[1], vp[2]);
						
					if (attrib.normals.size() > 0)
					{
						float* np = &attrib.normals[3 * index.normal_index];
						att.norm = glm::vec3(np[0], np[1], np[2]);
					}

					if (attrib.colors.size() > 0)
					{
						float* cp = &attrib.colors[3 * index.vertex_index];
						att.color = glm::clamp(glm::vec3(cp[0], cp[1], cp[2]), 0.0f, 1.0f);
					}

					if (attrib.texcoords.size() > 0)
					{
						float* tp = &attrib.texcoords[2 * index.texcoord_index];
						att.uv = glm::vec2(tp[0], 1.0f - tp[1]);
					}
						
					int idx;
					uint64_t hash = crc64(0, (unsigned char*)&att, sizeof(Attributes));

					auto iter = ind_map.find(hash);
					if (iter == ind_map.end())
					{
						idx = (int)prim_out.positions.size();
						prim_out.positions.push_back(att.pos);
						if (attrib.normals.size() > 0)
						{
							prim_out.normals.push_back(att.norm);
						}
						if (attrib.colors.size() > 0)
						{
							prim_out.colors.push_back(att.color);
						}
						if (attrib.texcoords.size() > 0)
						{
							prim_out.texcoords.push_back(att.uv);
						}
						ind_map[hash] = idx;
					}
					else
					{
						idx = ind_map[hash];
					}
					cur_ind[k] = idx;

				}
				prim_out.indices.push_back(cur_ind);
			}
		}
	}

	//////////////////////////// Write GLTF //////////////////////////

	tinygltf::Model m_out;
	m_out.scenes.resize(1);
	tinygltf::Scene& scene_out = m_out.scenes[0];
	scene_out.name = "Scene";

	m_out.asset.version = "2.0";
	m_out.asset.generator = "tinygltf";

	m_out.buffers.resize(1);
	tinygltf::Buffer& buf_out = m_out.buffers[0];

	size_t offset = 0;
	size_t length = 0;
	size_t view_id = 0;
	size_t acc_id = 0;

	// sampler
	m_out.samplers.resize(1);
	tinygltf::Sampler& sampler = m_out.samplers[0];
	sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
	sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;

	// texture
	int num_textures = (int)textures.size();
	m_out.images.resize(num_textures);
	m_out.textures.resize(num_textures);

	for (int i = 0; i < num_textures; i++)
	{
		Image& tex_in = textures[i];
		tinygltf::Image& img_out = m_out.images[i];
		tinygltf::Texture& tex_out = m_out.textures[i];

		img_out.name = tex_in.name;
		img_out.width = tex_in.width;
		img_out.height = tex_in.height;
		img_out.component = 4;
		img_out.bits = 8;
		img_out.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;

		if (tex_in.mimeType == "image/png")
		{
			img_out.mimeType = "image/png";

			std::vector<unsigned char>& png_buf = tex_in.storage;
			if (png_buf.size() == 0)
			{
				stbi_write_png_to_func([](void* context, void* data, int size)
					{
						std::vector<unsigned char>* buf = (std::vector<unsigned char>*)context;
				size_t offset = buf->size();
				buf->resize(offset + size);
				memcpy(buf->data() + offset, data, size);
					}, &png_buf, img_out.width, img_out.height, 4, tex_in.data.data(), img_out.width * 4);
			}

			offset = buf_out.data.size();
			length = png_buf.size();
			buf_out.data.resize((offset + length + 3) / 4 * 4);
			memcpy(buf_out.data.data() + offset, png_buf.data(), length);
		}
		else if (tex_in.mimeType == "image/jpeg")
		{
			img_out.mimeType = "image/jpeg";

			std::vector<unsigned char>& jpg_buf = tex_in.storage;
			if (jpg_buf.size() == 0)
			{
				stbi_write_jpg_to_func([](void* context, void* data, int size)
					{
						std::vector<unsigned char>* buf = (std::vector<unsigned char>*)context;
				size_t offset = buf->size();
				buf->resize(offset + size);
				memcpy(buf->data() + offset, data, size);
					}, &jpg_buf, img_out.width, img_out.height, 4, tex_in.data.data(), 80);
			}
			offset = buf_out.data.size();
			length = jpg_buf.size();
			buf_out.data.resize((offset + length + 3) / 4 * 4);
			memcpy(buf_out.data.data() + offset, jpg_buf.data(), length);
		}

		view_id = m_out.bufferViews.size();
		{
			tinygltf::BufferView view;
			view.buffer = 0;
			view.byteOffset = offset;
			view.byteLength = length;
			m_out.bufferViews.push_back(view);
		}
		img_out.bufferView = view_id;

		tex_out.name = tex_in.name;
		tex_out.sampler = 0;
		tex_out.source = i;
	}

	// material	
	m_out.materials.resize(num_materials);
	for (int i = 0; i < num_materials; i++)
	{
		Material& material_mid = materials_mid[i];
		tinygltf::Material& material_out = m_out.materials[i];
		material_out.name = material_mid.name;
		material_out.alphaMode = material_mid.blending ? "BLEND" : "OPAQUE";
		material_out.pbrMetallicRoughness.roughnessFactor = material_mid.roughnessFactor;
		material_out.pbrMetallicRoughness.metallicFactor = material_mid.metallicFactor;
		glm::vec4 baseColorFactor = material_mid.baseColorFactor;
		material_out.pbrMetallicRoughness.baseColorFactor = { baseColorFactor.x, baseColorFactor.y,baseColorFactor.z, baseColorFactor.w };
		material_out.doubleSided = true;
		material_out.pbrMetallicRoughness.baseColorTexture.index = material_mid.baseColorTex;
		glm::vec3 emissiveFactor = material_mid.emissionFactor;
		material_out.emissiveFactor = { emissiveFactor.x, emissiveFactor.y, emissiveFactor.z };
		if (material_mid.strength_emission > 1.0f)
		{
			tinygltf::Value::Object emissive_stength;
			emissive_stength["emissiveStrength"] = tinygltf::Value(material_mid.strength_emission);
			material_out.extensions["KHR_materials_emissive_strength"] = tinygltf::Value(emissive_stength);
		}
		material_out.emissiveTexture.index = material_mid.emissiveTex;
		material_out.normalTexture.index = material_mid.normalTex;
	}

	m_out.nodes.resize(num_meshes +1);
	m_out.meshes.resize(num_meshes);

	{
		tinygltf::Node& node_out = m_out.nodes[0];
		node_out.name = "scene";
		node_out.translation = { 0.0, 0.0, 0.0 };
		node_out.rotation = { 0.0, 0.0, 0.0, 1.0 };
		node_out.scale = { 1.0, 1.0, 1.0 };
		node_out.children.resize(num_meshes);
		for (size_t i = 0; i < num_meshes; i++)
		{
			node_out.children[i] = (int)(i + 1);
		}

	}
	scene_out.nodes.push_back(0);

	for (size_t i = 0; i < num_meshes; i++)
	{
		Mesh& mesh_in = meshes[i];
		tinygltf::Node& node_out = m_out.nodes[i+1];
		tinygltf::Mesh& mesh_out = m_out.meshes[i];
		node_out.name = mesh_in.name;
		node_out.translation = { 0.0, 0.0, 0.0 };
		node_out.rotation = { 0.0, 0.0, 0.0, 1.0 };
		node_out.scale = { 1.0, 1.0, 1.0 };
		mesh_out.name = mesh_in.name;
		node_out.mesh = i;

		int num_material = (int)mesh_in.primitives.size();
		mesh_out.name = mesh_in.name;
		mesh_out.primitives.resize(num_material);

		for (int j = 0; j < num_material; j++)
		{
			Primitive& prim_in = mesh_in.primitives[j];
			tinygltf::Primitive& prim_out = mesh_out.primitives[j];
			prim_out.material = prim_in.material;
			prim_out.mode = TINYGLTF_MODE_TRIANGLES;

			int num_pos = (int)prim_in.positions.size();
			int num_face = (int)prim_in.indices.size();

			offset = buf_out.data.size();
			length = sizeof(glm::ivec3) * num_face;
			buf_out.data.resize(offset + length);
			memcpy(buf_out.data.data() + offset, prim_in.indices.data(), length);

			view_id = m_out.bufferViews.size();
			{
				tinygltf::BufferView view;
				view.buffer = 0;
				view.byteOffset = offset;
				view.byteLength = length;
				view.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
				m_out.bufferViews.push_back(view);
			}

			acc_id = m_out.accessors.size();
			{
				tinygltf::Accessor acc;
				acc.bufferView = view_id;
				acc.byteOffset = 0;
				acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
				acc.count = (size_t)(num_face * 3);
				acc.type = TINYGLTF_TYPE_SCALAR;
				acc.maxValues = { (double)(num_pos - 1) };
				acc.minValues = { 0 };
				m_out.accessors.push_back(acc);
			}

			prim_out.indices = acc_id;

			glm::vec3 min_pos = { FLT_MAX, FLT_MAX, FLT_MAX };
			glm::vec3 max_pos = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

			for (int k = 0; k < num_pos; k++)
			{
				glm::vec3 pos = prim_in.positions[k];
				if (pos.x < min_pos.x) min_pos.x = pos.x;
				if (pos.x > max_pos.x) max_pos.x = pos.x;
				if (pos.y < min_pos.y) min_pos.y = pos.y;
				if (pos.y > max_pos.y) max_pos.y = pos.y;
				if (pos.z < min_pos.z) min_pos.z = pos.z;
				if (pos.z > max_pos.z) max_pos.z = pos.z;
			}


			offset = buf_out.data.size();
			length = sizeof(glm::vec3) * num_pos;
			buf_out.data.resize(offset + length);
			memcpy(buf_out.data.data() + offset, prim_in.positions.data(), length);

			view_id = m_out.bufferViews.size();
			{
				tinygltf::BufferView view;
				view.buffer = 0;
				view.byteOffset = offset;
				view.byteLength = length;
				view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
				m_out.bufferViews.push_back(view);
			}

			acc_id = m_out.accessors.size();
			{
				tinygltf::Accessor acc;
				acc.bufferView = view_id;
				acc.byteOffset = 0;
				acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
				acc.count = (size_t)(num_pos);
				acc.type = TINYGLTF_TYPE_VEC3;
				acc.maxValues = { max_pos.x, max_pos.y, max_pos.z };
				acc.minValues = { min_pos.x, min_pos.y, min_pos.z };
				m_out.accessors.push_back(acc);
			}

			prim_out.attributes["POSITION"] = acc_id;

			if (prim_in.normals.size() > 0)
			{
				offset = buf_out.data.size();
				length = sizeof(glm::vec3) * num_pos;
				buf_out.data.resize(offset + length);
				memcpy(buf_out.data.data() + offset, prim_in.normals.data(), length);

				view_id = m_out.bufferViews.size();
				{
					tinygltf::BufferView view;
					view.buffer = 0;
					view.byteOffset = offset;
					view.byteLength = length;
					view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
					m_out.bufferViews.push_back(view);
				}

				acc_id = m_out.accessors.size();
				{
					tinygltf::Accessor acc;
					acc.bufferView = view_id;
					acc.byteOffset = 0;
					acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					acc.count = (size_t)(num_pos);
					acc.type = TINYGLTF_TYPE_VEC3;
					m_out.accessors.push_back(acc);
				}

				prim_out.attributes["NORMAL"] = acc_id;
			}

			if (prim_in.colors.size() > 0)
			{
				offset = buf_out.data.size();
				length = sizeof(glm::vec3) * num_pos;
				buf_out.data.resize(offset + length);
				memcpy(buf_out.data.data() + offset, prim_in.colors.data(), length);

				view_id = m_out.bufferViews.size();
				{
					tinygltf::BufferView view;
					view.buffer = 0;
					view.byteOffset = offset;
					view.byteLength = length;
					view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
					m_out.bufferViews.push_back(view);
				}

				acc_id = m_out.accessors.size();
				{
					tinygltf::Accessor acc;
					acc.bufferView = view_id;
					acc.byteOffset = 0;
					acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					acc.count = (size_t)(num_pos);
					acc.type = TINYGLTF_TYPE_VEC3;
					m_out.accessors.push_back(acc);
				}

				prim_out.attributes["COLOR_0"] = acc_id;

			}

			if (prim_in.texcoords.size() > 0)
			{
				offset = buf_out.data.size();
				length = sizeof(glm::vec2) * num_pos;
				buf_out.data.resize(offset + length);
				memcpy(buf_out.data.data() + offset, prim_in.texcoords.data(), length);

				view_id = m_out.bufferViews.size();
				{
					tinygltf::BufferView view;
					view.buffer = 0;
					view.byteOffset = offset;
					view.byteLength = length;
					view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
					m_out.bufferViews.push_back(view);
				}

				acc_id = m_out.accessors.size();
				{
					tinygltf::Accessor acc;
					acc.bufferView = view_id;
					acc.byteOffset = 0;
					acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					acc.count = (size_t)(num_pos);
					acc.type = TINYGLTF_TYPE_VEC2;
					m_out.accessors.push_back(acc);
				}

				prim_out.attributes["TEXCOORD_0"] = acc_id;
			}
		}
	}

	tinygltf::TinyGLTF gltf;
	gltf.WriteGltfSceneToFile(&m_out, argv[2], true, true, false, true);

	return 0;
}

