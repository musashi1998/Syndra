#include "lpch.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include <glad/glad.h>

namespace Syndra {

	SceneRenderer::SceneData* SceneRenderer::s_Data;

	void SceneRenderer::Initialize()
	{
		s_Data = new SceneRenderer::SceneData;
		FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8 , FramebufferTextureFormat::DEPTH24STENCIL8 };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		fbSpec.Samples = 4;
		s_Data->mainFB = FrameBuffer::Create(fbSpec);
		fbSpec.Samples = 1;
		s_Data->postProcFB = FrameBuffer::Create(fbSpec);
		fbSpec.Attachments = { FramebufferTextureFormat::RED_INTEGER , FramebufferTextureFormat::DEPTH24STENCIL8 };
		s_Data->mouseFB = FrameBuffer::Create(fbSpec);

		if (!s_Data->aa) {
			s_Data->shaders.Load("assets/shaders/diffuse.glsl");
			s_Data->shaders.Load("assets/shaders/main.glsl");
			//s_Data->shaders.Load("assets/shaders/mouse.glsl");
			//s_Data->shaders.Load("assets/shaders/outline.glsl");
		}
		s_Data->aa = Shader::Create("assets/shaders/aa.glsl");
		s_Data->diffuse = s_Data->shaders.Get("diffuse");
		s_Data->main = s_Data->shaders.Get("main");
		//sample code
		for (auto& member : s_Data->diffuse->GetPushConstants())
		{
			for (auto& mem : member.members)
			{
				SN_CORE_ERROR("Diffuse shader members: {0} {1}", mem.name, mem.size);
			}
		}

		for (auto& sampler : s_Data->diffuse->GetSamplers())
		{
			SN_CORE_ERROR("Sampler : {0}, set={1}, binding{2}", sampler.name, sampler.set, sampler.binding);
		}
		//s_Data->mouseShader = s_Data->shaders.Get("mouse");
		//s_Data->outline = s_Data->shaders.Get("outline");

		s_Data->clearColor = glm::vec3(0.196f, 0.196f, 0.196f);

		s_Data->screenVao = VertexArray::Create();
		float quad[] = {
			 1.0f,  1.0f, 0.0f,    1.0f, 1.0f,   // top right
			 1.0f, -1.0f, 0.0f,    1.0f, 0.0f,   // bottom right
			-1.0f, -1.0f, 0.0f,    0.0f, 0.0f,   // bottom left
			-1.0f,  1.0f, 0.0f,    0.0f, 1.0f    // top left 
		};
		s_Data->screenVbo = VertexBuffer::Create(quad, sizeof(quad));
		BufferLayout quadLayout = {
			{ShaderDataType::Float3,"a_pos"},
			{ShaderDataType::Float2,"a_uv"},
		};
		s_Data->screenVbo->SetLayout(quadLayout);
		s_Data->screenVao->AddVertexBuffer(s_Data->screenVbo);
		unsigned int quadIndices[] = {
			0, 3, 1, // first triangle
			1, 3, 2  // second triangle
		};
		s_Data->screenEbo = IndexBuffer::Create(quadIndices, sizeof(quadIndices) / sizeof(uint32_t));
		s_Data->screenVao->SetIndexBuffer(s_Data->screenEbo);
		s_Data->CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
		s_Data->TransformUniformBuffer = UniformBuffer::Create(sizeof(Transform), 1);
		SN_CORE_TRACE("SIZE OF POINTLIGHTS : {0}", sizeof(s_Data->pointLights));
		s_Data->PointLightsBuffer = UniformBuffer::Create(sizeof(s_Data->pointLights), 2);
		s_Data->DirLightBuffer = UniformBuffer::Create(sizeof(s_Data->dirLight), 3);
	}

	void SceneRenderer::BeginScene(const PerspectiveCamera& camera)
	{
		s_Data->CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data->CameraBuffer.position = glm::vec4(camera.GetPosition(), 0);
		s_Data->CameraUniformBuffer->SetData(&s_Data->CameraBuffer, sizeof(CameraData));

		for (auto pointLight : s_Data->pointLights) {
			pointLight.color = glm::vec4(0);
			pointLight.position = glm::vec4(0);
			pointLight.dist = 0.0f;
		}

		s_Data->dirLight.color = glm::vec4(0);
		s_Data->dirLight.position = glm::vec4(0);
		s_Data->dirLight.direction = glm::vec4(0);

		s_Data->PointLightsBuffer->SetData(&s_Data->pointLights, sizeof(s_Data->pointLights));
		s_Data->DirLightBuffer->SetData(&s_Data->dirLight, sizeof(directionalLight));

		s_Data->TransformBuffer.lightPos = glm::vec4(camera.GetPosition(), 0);
		s_Data->mainFB->Bind();
		RenderCommand::SetState(RenderState::DEPTH_TEST, true);
		RenderCommand::SetClearColor(glm::vec4(s_Data->clearColor, 1.0f));
		RenderCommand::Clear();

		Renderer::BeginScene(camera);

	}

	void SceneRenderer::RenderScene(Scene& scene)
	{
		auto viewLights = scene.m_Registry.view<TransformComponent, LightComponent>();
		int pIndex = 0;


		for (auto ent : viewLights)
		{
			auto& tc = viewLights.get<TransformComponent>(ent);
			auto& lc = viewLights.get<LightComponent>(ent);

			if (lc.type == LightType::Directional) {
				auto p = dynamic_cast<DirectionalLight*>(lc.light.get());
				s_Data->dirLight.color = glm::vec4(lc.light->GetColor(),0);
				s_Data->dirLight.direction = glm::vec4(p->GetDirection(),0);
				s_Data->dirLight.position = glm::vec4(tc.Translation, 0);
			}
			if (lc.type == LightType::Point) {
				if (pIndex < 4) {
					auto p = dynamic_cast<PointLight*>(lc.light.get());
					s_Data->pointLights[pIndex].color = glm::vec4(p->GetColor(), 1);
					s_Data->pointLights[pIndex].position = glm::vec4(tc.Translation, 1);
					s_Data->pointLights[pIndex].dist = p->GetRange();
					pIndex++;
				}
			}
		}
		s_Data->PointLightsBuffer->SetData(&s_Data->pointLights, sizeof(s_Data->pointLights));
		s_Data->DirLightBuffer->SetData(&s_Data->dirLight, sizeof(directionalLight));

		auto view = scene.m_Registry.view<TransformComponent, MeshComponent>();
		s_Data->mainFB->Bind();
		for (auto ent : view)
		{
			auto& tc = view.get<TransformComponent>(ent);
			auto& mc = view.get<MeshComponent>(ent);
			if (!mc.path.empty()) {
				s_Data->TransformBuffer.transform = tc.GetTransform();
				s_Data->TransformUniformBuffer->SetData(&s_Data->TransformBuffer, sizeof(Transform));
				s_Data->ShaderBuffer.col = glm::vec4(0.5f);
				if (scene.m_Registry.has<MaterialComponent>(ent)){
					auto& mat = scene.m_Registry.get<MaterialComponent>(ent);
					SceneRenderer::RenderEntityColor(ent, tc, mc, mat);
				}
				else 
				{
					SceneRenderer::RenderEntityColor(ent, tc, mc);
				}
			}
		}
		s_Data->mainFB->Unbind();

		//s_Data->mouseFB->Bind();
		//RenderCommand::Clear();
		//s_Data->mouseFB->ClearAttachment(0, -1);
		//RenderCommand::SetState(RenderState::DEPTH_TEST, true);

		//for (auto ent : view)
		//{
		//	auto& tc = view.get<TransformComponent>(ent);
		//	auto& mc = view.get<MeshComponent>(ent);
		//	//TODO material
		//	s_Data->mouseShader->Bind();
		//	SceneRenderer::RenderEntityID(ent, tc, mc);
		//}
		//s_Data->mouseFB->Unbind();
	}

	void SceneRenderer::RenderEntityColor(const entt::entity& entity, TransformComponent& tc, MeshComponent& mc)
	{
		//--------------------------------------------------color and outline pass------------------------------------------------//

		//s_Data->diffuse->SetFloat4("push.col", glm::vec4(0.5f));
		//TODO material system
		//m_Texture->Bind(0);
		//s_Data->diffuse->SetMat4("u_trans", tc.GetTransform());
		//s_Data->diffuse->SetFloat3("cameraPos", s_Data->CameraBuffer.position);
		//s_Data->diffuse->SetFloat3("lightPos", s_Data->CameraBuffer.position);
		//difShader->SetFloat3("cubeCol", m_CubeColor);
		//glEnable(GL_DEPTH_TEST);
		//TODO add selected entities
		//if (entity.IsSelected())
		//{
			//s_Data->outline->Bind();
			//auto transform = tc;
			//transform.Scale += glm::vec3(.05f);
			//s_Data->outline->SetMat4("u_trans", transform.GetTransform());
			//glDisable(GL_DEPTH_TEST);
			//Renderer::Submit(s_Data->outline, mc.model);
			//RenderCommand::SetState(GL_DEPTH_TEST, false);
		//}
		//glEnable(GL_DEPTH_TEST);
		RenderCommand::SetState(RenderState::CULL, false);
		Renderer::Submit(s_Data->diffuse, mc.model);
		RenderCommand::SetState(RenderState::CULL, true);
	}

	void SceneRenderer::RenderEntityColor(const entt::entity& entity, TransformComponent& tc, MeshComponent& mc, MaterialComponent& mat)
	{
		Renderer::Submit(mat.material, mc.model);
	}

	void SceneRenderer::RenderEntityID(const entt::entity& entity, TransformComponent& tc, MeshComponent& mc)
	{
		//-------------------------------------------------entity id pass--------------------------------------------------------//

		//s_Data->mouseShader->SetMat4("u_trans", tc.GetTransform());
		//s_Data->mouseShader->SetInt("u_ID", (uint32_t)entity);
		//Renderer::Submit(s_Data->mouseShader, mc.model);
		//s_Data->mouseShader->Unbind();
	}

	void SceneRenderer::EndScene()
	{
		//-------------------------------------------------post-processing pass---------------------------------------------------//

		s_Data->postProcFB->Bind();
		RenderCommand::SetState(RenderState::SRGB, true);
		RenderCommand::Clear();
		s_Data->screenVao->Bind();
		RenderCommand::SetState(RenderState::DEPTH_TEST, false);
		s_Data->aa->Bind();
		Texture2D::BindTexture(s_Data->mainFB->GetColorAttachmentRendererID(), 0);

		Renderer::Submit(s_Data->aa, s_Data->screenVao);

		s_Data->aa->Unbind();
		RenderCommand::SetState(RenderState::SRGB, false);
		s_Data->postProcFB->Unbind();
		
		Renderer::EndScene();
	}

	void SceneRenderer::Reload()
	{
		s_Data->shaders.DeleteShader(s_Data->diffuse);
		s_Data->diffuse = s_Data->shaders.Load("assets/shaders/diffuse.glsl");
	}

	void SceneRenderer::OnViewPortResize(uint32_t width, uint32_t height)
	{
		s_Data->mainFB->Resize(width, height);
		s_Data->postProcFB->Resize(width, height);
		s_Data->mouseFB->Resize(width, height);
	}

}