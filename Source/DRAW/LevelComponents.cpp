#include "DrawComponents.h"
#include "../GAME/GameComponents.h"
#include "../CCL.h"
#include "../UTIL/Utilities.h"

namespace DRAW
{

	void Construct_CPULevel(entt::registry& registry, entt::entity entity) {
		CPULevel& cpuLevel = registry.get<CPULevel>(entity);

		GW::SYSTEM::GLog log;
		log.EnableConsoleLogging(1);

		bool success = cpuLevel.levelData.LoadLevel(cpuLevel.levelFile.c_str(), cpuLevel.modelPath.c_str(), log);

		if (!success) throw std::runtime_error("DEBUG: Unable to load the level data to the CPU.");
	};


	void Construct_GPULevel(entt::registry& registry, entt::entity entity) {
		CPULevel& cpuLevel = registry.get<CPULevel>(entity);
		Level_Data level = cpuLevel.levelData;

		registry.emplace<VulkanVertexBuffer>(entity);
		registry.emplace<VulkanIndexBuffer>(entity);

		registry.emplace<std::vector<H2B::VERTEX>>(entity, level.levelVertices);
		registry.emplace<std::vector<unsigned>>(entity, level.levelIndices);

		registry.patch<VulkanVertexBuffer>(entity);
		registry.patch<VulkanIndexBuffer>(entity);

		for (Level_Data::BLENDER_OBJECT obj : level.blenderObjects)
		{
			MeshCollection collection;
			Level_Data::LEVEL_MODEL model = level.levelModels[obj.modelIndex];

			collection.obb = level.levelColliders[model.colliderIndex];

			for (unsigned i = 0; i < model.meshCount; ++i)
			{
				entt::entity ent = registry.create();

				H2B::MESH mesh = level.levelMeshes[model.meshStart + i];

				if (model.isDynamic)
				{
					collection.entities.push_back(ent);
					registry.emplace<DoNotRender>(ent);
				}

				registry.emplace<GeometryData>(ent, GeometryData{
					model.indexStart + mesh.drawInfo.indexOffset,
					mesh.drawInfo.indexCount,
					model.vertexStart
					});

				auto materialIndex = mesh.materialIndex + model.materialStart;
				auto attrib = level.levelMaterials[materialIndex].attrib;
				const Level_Data::MATERIAL_TEXTURES& textures = level.levelTextures[materialIndex];

				attrib.albedoIndex = textures.albedoIndex;

				registry.emplace<GPUInstance>(ent, GPUInstance{
					level.levelTransforms[obj.transformIndex],
					attrib,
					textures.albedoIndex,
					textures.normalIndex,
					textures.emissiveIndex,
					textures.alphaIndex,
					textures.specularIndex
					});

			}
			if (model.isCollidable)
			{
				entt::entity ent = registry.create();
				registry.emplace<GAME::Obstacle>(ent);
				registry.emplace<GAME::Collidable>(ent);
				registry.emplace<MeshCollection>(ent, collection);
				registry.emplace<GAME::Transform>(ent, level.levelTransforms[obj.transformIndex]);
			}
			if (model.isDynamic)
			{
				ModelManager& modelManager = registry.ctx().get<ModelManager>();
				modelManager.models.insert({ obj.blendername, collection });
			}
		}
	};

	void Destroy_MeshCollection(entt::registry& registry, entt::entity entity) {
		MeshCollection mesh = registry.get<MeshCollection>(entity);
		for (auto ent : mesh.entities) registry.destroy(ent);
	}

	CONNECT_COMPONENT_LOGIC()
	{
		registry.on_construct<CPULevel>().connect<Construct_CPULevel>();
		registry.on_construct<GPULevel>().connect<Construct_GPULevel>();
		registry.on_destroy<MeshCollection>().connect<Destroy_MeshCollection>();
	};
};