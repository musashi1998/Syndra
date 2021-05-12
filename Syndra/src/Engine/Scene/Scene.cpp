#include "lpch.h"
#include "Engine/Scene/Scene.h"

#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Components.h"

namespace Syndra {

	Scene::Scene(const std::string& name)
		:m_Name(name)
	{
		Entity::s_Scene = this;
		SceneRenderer::Initialize();
		m_Shaders = SceneRenderer::GetShaderLibrary();
		m_Camera = new PerspectiveCamera(45.0f, 1.66f, 0.1f, 1000.0f);
	}

	Scene::~Scene()
	{
		delete m_Camera;
	}

	Ref<Entity> Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create() };
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		entity.AddComponent<TransformComponent>();
		Ref<Entity> ent = CreateRef<Entity>(entity);
		m_Entities.push_back(ent);
		return ent;
	}

	Ref<Entity> Scene::CreateEntity(const Entity& other)
	{
		Entity entity = { m_Registry.create() };
		Ref<Entity> ent = CreateRef<Entity>(entity);
		ent->AddComponent<TagComponent>(other.GetComponent<TagComponent>());
		ent->AddComponent<TransformComponent>(other.GetComponent<TransformComponent>());
		if (other.HasComponent<CameraComponent>()) {
			ent->AddComponent<CameraComponent>(other.GetComponent<CameraComponent>());
		}
		if (other.HasComponent<MeshComponent>()) {
			ent->AddComponent<MeshComponent>(other.GetComponent<MeshComponent>());
		}
		if (other.HasComponent<MaterialComponent>()) {
			ent->AddComponent<MaterialComponent>(other.GetComponent<MaterialComponent>());
		}
		m_Entities.push_back(ent);
		return ent;
	}

	void Scene::DestroyEntity(const Entity& entity)
	{
		m_Registry.destroy(entity);
		for (auto& e : m_Entities) {
			if (*e == entity) {
				auto it = std::find(m_Entities.begin(), m_Entities.end(), e);
				if (it != m_Entities.end()) {
					m_Entities.erase(it);
					break;
				}
			}
		}

	}

	Entity Scene::FindEntity(uint32_t id)
	{
		for (auto& e : m_Entities) {
			if (*e == (entt::entity)id) {
				return *e;
			}
		}
		return {};
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{

	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		SceneRenderer::BeginScene(*m_Camera);
		SceneRenderer::RenderScene(*this);
		SceneRenderer::EndScene();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_Camera->SetViewportSize(width, height);
		SceneRenderer::OnViewPortResize(width, height);
		// Resize our non-FixedAspectRatio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		static_assert(false);
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<MaterialComponent>(Entity entity, MaterialComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<PointLightComponent>(Entity entity, PointLightComponent& component)
	{
	}

}
