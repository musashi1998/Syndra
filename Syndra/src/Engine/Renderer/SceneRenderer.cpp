#include "lpch.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "Engine/Renderer/SceneRenderer.h"

#include "Engine/Utils/PlatformUtils.h"
#include "Engine/Core/Application.h"

#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"

#include "Engine/Utils/PoissonGenerator.h"
#include <glad/glad.h>

namespace Syndra {

	static SceneRenderer::SceneData s_Data;

	void GeneratePoissonDisk(Ref<Texture1D>& sampler, size_t numSamples) {
		PoissonGenerator::DefaultPRNG PRNG;
		size_t attempts = 0;
		auto points = PoissonGenerator::generatePoissonPoints(numSamples * 2, PRNG);
		while (points.size() < numSamples && ++attempts < 100)
			auto points = PoissonGenerator::generatePoissonPoints(numSamples * 2, PRNG);
		if (attempts == 100)
		{
			SN_CORE_ERROR("couldn't generate Poisson-disc distribution with {0} samples", numSamples);
			numSamples = points.size();
		}
		std::vector<float> data(numSamples * 2);
		for (auto i = 0, j = 0; i < numSamples; i++, j += 2)
		{
			auto& point = points[i];
			data[j] = point.x;
			data[j + 1] = point.y;
		}

		sampler = Texture1D::Create(numSamples, &data[0]);
	}

	void SceneRenderer::Initialize()
	{

		//------------------------------------------------Deferred Geometry Render Pass----------------------------------------//
		FramebufferSpecification GeoFbSpec;
		GeoFbSpec.Attachments =
		{
			FramebufferTextureFormat::RGBA16F,			// Position texture attachment
			FramebufferTextureFormat::RGBA16F,			// Normal texture attachment
			FramebufferTextureFormat::RGBA16F,			// Albedo texture attachment
			FramebufferTextureFormat::RGBA16F,		    // Roughness-Metallic-AO texture attachment
			FramebufferTextureFormat::RED_INTEGER,		// Entities ID texture attachment
			FramebufferTextureFormat::DEPTH24STENCIL8	// default depth map
		};
		GeoFbSpec.Width = 1280;
		GeoFbSpec.Height = 720;
		GeoFbSpec.Samples = 1;
		GeoFbSpec.ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		RenderPassSpecification GeoPassSpec;
		GeoPassSpec.TargetFrameBuffer = FrameBuffer::Create(GeoFbSpec);
		s_Data.geoPass = RenderPass::Create(GeoPassSpec);

		//-------------------------------------------Lighting and Post Processing Pass---------------------------//
		FramebufferSpecification postProcFB;
		postProcFB.Attachments = { FramebufferTextureFormat::RGBA8 , FramebufferTextureFormat::DEPTH24STENCIL8 };
		postProcFB.Width = 1280;
		postProcFB.Height = 720;
		postProcFB.Samples = 1;
		postProcFB.ClearColor = glm::vec4(0.196f, 0.196f, 0.196f, 1.0f);

		RenderPassSpecification finalPassSpec;
		finalPassSpec.TargetFrameBuffer = FrameBuffer::Create(postProcFB);
		s_Data.lightingPass = RenderPass::Create(finalPassSpec);

		//-----------------------------------------------Shadow Pass---------------------------------------------//

		//Directional Light shadow map
		FramebufferSpecification shadowSpec;
		shadowSpec.Attachments = { FramebufferTextureFormat::DEPTH32 };
		shadowSpec.Width = 4096;
		shadowSpec.Height = 4096;
		shadowSpec.Samples = 1;
		shadowSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		RenderPassSpecification shadowPassSpec;
		shadowPassSpec.TargetFrameBuffer = FrameBuffer::Create(shadowSpec);
		s_Data.shadowPass = RenderPass::Create(shadowPassSpec);

		//-----------------------------------------------Anti Aliasing------------------------------------------//
		FramebufferSpecification aaFB;
		aaFB.Attachments = { FramebufferTextureFormat::RGBA8 };
		aaFB.Width = 1280;
		aaFB.Height = 720;
		aaFB.Samples = 1;
		aaFB.ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		RenderPassSpecification aaPassSpec;
		aaPassSpec.TargetFrameBuffer = FrameBuffer::Create(aaFB);
		s_Data.aaPass = RenderPass::Create(aaPassSpec);

		//------------------------------------------------Shaders-----------------------------------------------//

		//s_Data.fxaa = Shader::Create("assets/shaders/FXAA.glsl");

		if (!s_Data.main) {
			s_Data.shaders.Load("assets/shaders/diffuse.glsl");
			s_Data.shaders.Load("assets/shaders/FXAA.glsl");
			s_Data.shaders.Load("assets/shaders/main.glsl");
			s_Data.shaders.Load("assets/shaders/DeferredLighting.glsl");
			s_Data.shaders.Load("assets/shaders/GeometryPass.glsl");
			//s_Data.shaders.Load("assets/shaders/mouse.glsl");
			//s_Data.shaders.Load("assets/shaders/outline.glsl");
		}
		s_Data.depth = Shader::Create("assets/shaders/depth.glsl");
		s_Data.geoShader = s_Data.shaders.Get("GeometryPass");
		s_Data.fxaa = s_Data.shaders.Get("FXAA");
		s_Data.diffuse = s_Data.shaders.Get("diffuse");
		s_Data.main = s_Data.shaders.Get("main");
		s_Data.deferredLighting = s_Data.shaders.Get("DeferredLighting");

		//----------------------------------------------SCREEN QUAD---------------------------------------------//
		s_Data.screenVao = VertexArray::Create();
		float quad[] = {
			 1.0f,  1.0f, 0.0f,    1.0f, 1.0f,   // top right
			 1.0f, -1.0f, 0.0f,    1.0f, 0.0f,   // bottom right
			-1.0f, -1.0f, 0.0f,    0.0f, 0.0f,   // bottom left
			-1.0f,  1.0f, 0.0f,    0.0f, 1.0f    // top left 
		};
		auto vb = VertexBuffer::Create(quad, sizeof(quad));
		BufferLayout quadLayout = {
			{ShaderDataType::Float3,"a_pos"},
			{ShaderDataType::Float2,"a_uv"},
		};
		vb->SetLayout(quadLayout);
		s_Data.screenVao->AddVertexBuffer(vb);
		unsigned int quadIndices[] = {
			0, 3, 1, // first triangle
			1, 3, 2  // second triangle
		};
		auto eb = IndexBuffer::Create(quadIndices, sizeof(quadIndices) / sizeof(uint32_t));
		s_Data.screenVao->SetIndexBuffer(eb);

		//----------------------------------------------Uniform BUffers---------------------------------------------//
		//TODO Should be moved to a different class?
		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);

		s_Data.exposure = 0.5f;
		s_Data.gamma = 1.9f;
		s_Data.lightSize = 1.0f;
		s_Data.orthoSize = 20.0f;
		s_Data.lightNear = 20.0f;
		s_Data.lightFar = 200.0f;
	
		//Light uniform Buffer layout: -- point lights -- spotlights -- directional light--Binding point 2
		s_Data.lightManager = CreateRef<LightManager>(2);

		GeneratePoissonDisk(s_Data.distributionSampler0, 64);
		GeneratePoissonDisk(s_Data.distributionSampler1, 64);

		s_Data.deferredLighting->Bind();
		s_Data.deferredLighting->SetFloat("pc.near", s_Data.lightNear);

		s_Data.diffuse->Bind();
		Texture1D::BindTexture(s_Data.distributionSampler0->GetRendererID(), 4);
		Texture1D::BindTexture(s_Data.distributionSampler1->GetRendererID(), 5);
		s_Data.diffuse->Unbind();

		float dSize = s_Data.orthoSize;
		s_Data.lightProj = glm::ortho(-dSize, dSize, -dSize, dSize, s_Data.lightNear, s_Data.lightFar);
		//s_Data.lightProj = glm::perspective(45.0f, 1.0f, s_Data.lightNear, s_Data.lightFar);
		s_Data.ShadowBuffer = UniformBuffer::Create(sizeof(glm::mat4)*25, 3);
		s_Data.intensity = 1.0f;

	}

	//Initializing camera, uniform buffers and environment map
	void SceneRenderer::BeginScene(const PerspectiveCamera& camera)
	{
		if (s_Data.environment)
		{
			s_Data.environment->SetViewProjection(camera.GetViewMatrix(), camera.GetProjection());
		}

		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.position = glm::vec4(camera.GetPosition(), 0);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(CameraData));

		s_Data.lightManager->IntitializeLights();
		Renderer::BeginScene(camera);
	}

	void SceneRenderer::UpdateLights()
	{
		auto viewLights = s_Data.scene->m_Registry.view<TransformComponent, LightComponent>();
		//point light index
		int pIndex = 0;
		//spot light index
		int sIndex = 0;
		//Set light values for each entity that has a light component
		for (auto ent : viewLights)
		{
			auto& tc = viewLights.get<TransformComponent>(ent);
			auto& lc = viewLights.get<LightComponent>(ent);

			if (lc.type == LightType::Directional) {
				auto p = dynamic_cast<DirectionalLight*>(lc.light.get());
				s_Data.lightManager->UpdateDirLight(p, tc.Translation);
				//shadow
				s_Data.lightView = glm::lookAt(-(glm::normalize(p->GetDirection()) * s_Data.lightFar / 4.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				s_Data.shadowData.lightViewProj = s_Data.lightProj * s_Data.lightView;
				s_Data.ShadowBuffer->SetData(&s_Data.shadowData, sizeof(glm::mat4));
				p = nullptr;
			}
			if (lc.type == LightType::Point) {
				if (pIndex < 4) {
					auto p = dynamic_cast<PointLight*>(lc.light.get());
					s_Data.lightManager->UpdatePointLights(p, tc.Translation, pIndex);
					pIndex++;
					p = nullptr;
				}
			}
			if (lc.type == LightType::Spot) {
				if (sIndex < 4) {
					auto p = dynamic_cast<SpotLight*>(lc.light.get());
					s_Data.lightManager->UpdateSpotLights(p, tc.Translation, sIndex);
					sIndex++;
					p = nullptr;
				}
			}
		}

		//Filling light buffer data with different light values
		s_Data.lightManager->UpdateBuffer();
	}

	void SceneRenderer::RenderScene()
	{

		UpdateLights();

		auto view = s_Data.scene->m_Registry.view<TransformComponent, MeshComponent>();
		//---------------------------------------------------------SHADOW PASS------------------------------------------//
		s_Data.shadowPass->BindTargetFrameBuffer();
		RenderCommand::SetState(RenderState::DEPTH_TEST, true);
		RenderCommand::SetClearColor(s_Data.shadowPass->GetSpecification().TargetFrameBuffer->GetSpecification().ClearColor);
		s_Data.depth->Bind();
		RenderCommand::Clear();
		for (auto ent : view)
		{
			auto& tc = view.get<TransformComponent>(ent);
			auto& mc = view.get<MeshComponent>(ent);
			if (!mc.path.empty())
			{
				s_Data.depth->SetMat4("transform.u_trans", tc.GetTransform());
				SceneRenderer::RenderEntity(ent, mc, s_Data.depth);
			}
		}
		s_Data.shadowPass->UnbindTargetFrameBuffer();

		//--------------------------------------------------GEOMETRY PASS----------------------------------------------//
		s_Data.geoPass->BindTargetFrameBuffer();
		RenderCommand::SetState(RenderState::DEPTH_TEST, true);
		RenderCommand::SetClearColor(s_Data.geoPass->GetSpecification().TargetFrameBuffer->GetSpecification().ClearColor);
		s_Data.geoPass->GetSpecification().TargetFrameBuffer->ClearAttachment(4, -1);
		s_Data.geoShader->Bind();
		RenderCommand::Clear();
		for (auto ent : view)
		{
			auto& tc = view.get<TransformComponent>(ent);
			auto& mc = view.get<MeshComponent>(ent);
			if (!mc.path.empty())
			{
				if (s_Data.scene->m_Registry.has<MaterialComponent>(ent)) {
					auto& mat = s_Data.scene->m_Registry.get<MaterialComponent>(ent);
					s_Data.geoShader->SetInt("transform.id", (uint32_t)ent);
					s_Data.geoShader->SetMat4("transform.u_trans", tc.GetTransform());
					SceneRenderer::RenderEntity(ent, mc, mat);
				}
				else
				{
					s_Data.geoShader->SetInt("push.HasAlbedoMap", 1);
					s_Data.geoShader->SetFloat("push.tiling", 1.0f);
					s_Data.geoShader->SetInt("push.HasNormalMap", 0);
					s_Data.geoShader->SetInt("push.HasMetallicMap", 0);
					s_Data.geoShader->SetInt("push.HasRoughnessMap", 0);
					s_Data.geoShader->SetInt("push.HasAOMap", 0);
					s_Data.geoShader->SetFloat("push.material.MetallicFactor", 0);
					s_Data.geoShader->SetFloat("push.material.RoughnessFactor", 1);
					s_Data.geoShader->SetFloat("push.material.AO", 1);
					s_Data.geoShader->SetMat4("transform.u_trans", tc.GetTransform());
					s_Data.geoShader->SetInt("transform.id", (uint32_t)ent);
					SceneRenderer::RenderEntity(ent, mc, s_Data.geoShader);
				}
			}
		}
		s_Data.geoShader->Unbind();
		s_Data.geoPass->UnbindTargetFrameBuffer();
	}

	void SceneRenderer::RenderEntity(const entt::entity& entity, MeshComponent& mc, const Ref<Shader>& shader)
	{
		//RenderCommand::SetState(RenderState::CULL, false);
		Renderer::Submit(shader, mc.model);
		//RenderCommand::SetState(RenderState::CULL, true);
	}

	void SceneRenderer::RenderEntity(const entt::entity& entity, MeshComponent& mc, MaterialComponent& mat)
	{
		Renderer::Submit(mat.m_Material, mc.model);
	}

	void SceneRenderer::EndScene()
	{
		//-------------------------------------------------Lighting and post-processing pass---------------------------------------------------//

		s_Data.lightingPass->BindTargetFrameBuffer();

		RenderCommand::SetState(RenderState::DEPTH_TEST, false);
		s_Data.screenVao->Bind();

		s_Data.deferredLighting->Bind();
		//shadow map samplers
		Texture2D::BindTexture(s_Data.shadowPass->GetSpecification().TargetFrameBuffer->GetDepthAttachmentRendererID(), 3);
		Texture1D::BindTexture(s_Data.distributionSampler0->GetRendererID(), 4);
		Texture1D::BindTexture(s_Data.distributionSampler1->GetRendererID(), 5);

		//Push constant variables
		s_Data.deferredLighting->SetFloat("pc.size", s_Data.lightSize * 0.0001f);
		s_Data.deferredLighting->SetInt("pc.numPCFSamples", s_Data.numPCF);
		s_Data.deferredLighting->SetInt("pc.numBlockerSearchSamples", s_Data.numBlocker);
		s_Data.deferredLighting->SetInt("pc.softShadow", (int)s_Data.softShadow);
		s_Data.deferredLighting->SetFloat("pc.exposure", s_Data.exposure);
		s_Data.deferredLighting->SetFloat("pc.gamma", s_Data.gamma);
		s_Data.deferredLighting->SetFloat("pc.near", s_Data.lightNear);
		s_Data.deferredLighting->SetFloat("pc.intensity", s_Data.intensity);
		//GBuffer samplers
		Texture2D::BindTexture(s_Data.geoPass->GetFrameBufferTextureID(0), 0);
		Texture2D::BindTexture(s_Data.geoPass->GetFrameBufferTextureID(1), 1);
		Texture2D::BindTexture(s_Data.geoPass->GetFrameBufferTextureID(2), 2);
		Texture2D::BindTexture(s_Data.geoPass->GetFrameBufferTextureID(3), 6);
		if (s_Data.environment) {
			s_Data.environment->SetIntensity(s_Data.intensity);
			s_Data.environment->BindIrradianceMap(7);
			s_Data.environment->BindPreFilterMap(8);
			s_Data.environment->BindBRDFMap(9);
		}
		Renderer::Submit(s_Data.deferredLighting, s_Data.screenVao);

		s_Data.deferredLighting->Unbind();

		s_Data.lightingPass->BindTargetFrameBuffer();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, s_Data.geoPass->GetSpecification().TargetFrameBuffer->GetRendererID());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_Data.lightingPass->GetSpecification().TargetFrameBuffer->GetRendererID()); // write to default framebuffer
		auto w = s_Data.lightingPass->GetSpecification().TargetFrameBuffer->GetSpecification().Width;
		auto h = s_Data.lightingPass->GetSpecification().TargetFrameBuffer->GetSpecification().Height;
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		if (s_Data.environment) {
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			s_Data.environment->RenderBackground();
			glDepthFunc(GL_LESS);
		}

		s_Data.lightingPass->UnbindTargetFrameBuffer();
		//-------------------------------------------------ANTI ALIASING PASS------------------------------------------------------------------//

		if (s_Data.useFxaa) {
			s_Data.aaPass->BindTargetFrameBuffer();
			s_Data.fxaa->Bind();
			s_Data.fxaa->SetFloat("pc.width", (float)s_Data.aaPass->GetSpecification().TargetFrameBuffer->GetSpecification().Width);
			s_Data.fxaa->SetFloat("pc.height", (float)s_Data.aaPass->GetSpecification().TargetFrameBuffer->GetSpecification().Height);
			Texture2D::BindTexture(s_Data.lightingPass->GetFrameBufferTextureID(0), 0);
			Renderer::Submit(s_Data.fxaa, s_Data.screenVao);
			s_Data.fxaa->Unbind();
			s_Data.aaPass->UnbindTargetFrameBuffer();
		}

		Renderer::EndScene();
	}

	void SceneRenderer::Reload(const Ref<Shader>& shader)
	{
		shader->Reload();
	}

	void SceneRenderer::OnViewPortResize(uint32_t width, uint32_t height)
	{
		s_Data.geoPass->GetSpecification().TargetFrameBuffer->Resize(width, height);
		s_Data.lightingPass->GetSpecification().TargetFrameBuffer->Resize(width, height);
		s_Data.aaPass->GetSpecification().TargetFrameBuffer->Resize(width, height);
	}

	void SceneRenderer::OnImGuiRender(bool* rendererOpen, bool* environmentOpen)
	{
		//Renderer settings
		if (*rendererOpen) {
			ImGui::Begin(ICON_FA_COGS" Renderer Settings", rendererOpen);

			ImGui::Text("Geometry pass debugger");
			static bool showAlbedo = false;
			static bool showNormal = false;
			static bool showPosition = false;
			static bool showRoughMetalAO = false;
			if (ImGui::Button("Albedo")) {
				showAlbedo = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Normal")) {
				showNormal = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Position")) {
				showPosition = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("RoughMetalAO")) {
				showRoughMetalAO = true;
			}
			auto width = s_Data.geoPass->GetSpecification().TargetFrameBuffer->GetSpecification().Width * 0.5f;
			auto height = s_Data.geoPass->GetSpecification().TargetFrameBuffer->GetSpecification().Height * 0.5f;
			auto ratio = height / width;
			width = ImGui::GetContentRegionAvail().x;
			height = ratio * width;
			if (showAlbedo) {
				ImGui::Begin("Albedo", &showAlbedo);
				width = ImGui::GetContentRegionAvail().x;
				height = ratio * width;
				ImGui::Image((ImTextureID)s_Data.geoPass->GetFrameBufferTextureID(2), {width, height}, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
				ImGui::End();
			}

			if (showNormal) {
				ImGui::Begin("Normal", &showNormal);
				width = ImGui::GetContentRegionAvail().x;
				height = ratio * width;
				ImGui::Image((ImTextureID)s_Data.geoPass->GetFrameBufferTextureID(1), { width, height }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
				ImGui::End();
			}

			if (showPosition) {
				ImGui::Begin("Position", &showPosition);
				width = ImGui::GetContentRegionAvail().x;
				height = ratio * width;
				ImGui::Image((ImTextureID)s_Data.geoPass->GetFrameBufferTextureID(0), { width, height }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
				ImGui::End();
			}

			if (showRoughMetalAO) {
				ImGui::Begin("RoughMetalAO", &showRoughMetalAO);
				width = ImGui::GetContentRegionAvail().x;
				height = ratio * width;
				ImGui::Image((ImTextureID)s_Data.geoPass->GetFrameBufferTextureID(3), { width, height }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
				ImGui::End();
			}
			ImGui::Separator();
			//V-Sync
			static bool vSync = true;
			ImGui::Checkbox("V-Sync", &vSync);
			Application::Get().GetWindow().SetVSync(vSync);
			ImGui::Separator();

			ImGui::Text("Anti Aliasing");
			ImGui::Checkbox("FXAA", &s_Data.useFxaa);
			ImGui::Separator();

			//Exposure
			ImGui::DragFloat("exposure", &s_Data.exposure, 0.01f, -2, 4);

			//Gamma
			ImGui::DragFloat("gamma", &s_Data.gamma, 0.01f, 0, 4);


			//shadow
			ImGui::Checkbox("Soft Shadow", &s_Data.softShadow);
			ImGui::DragFloat("PCF samples", &s_Data.numPCF, 1, 1, 64);
			ImGui::DragFloat("blocker samples", &s_Data.numBlocker, 1, 1, 64);
			ImGui::DragFloat("Light Size", &s_Data.lightSize, 0.01f, 0, 100);

			if (ImGui::DragFloat("Ortho Size", &s_Data.orthoSize, 0.1f, 1, 100)) {
				s_Data.lightProj = glm::ortho(-s_Data.orthoSize, s_Data.orthoSize, -s_Data.orthoSize, s_Data.orthoSize, s_Data.lightNear, s_Data.lightFar);
			}

			ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
			if (ImGui::DragFloat("near", &s_Data.lightNear, 0.01f, 0.1f, 100.0f)) {
				//s_Data.lightProj = glm::perspective(45.0f, 1.0f, s_Data.lightNear, s_Data.lightFar);
				s_Data.lightProj = glm::ortho(-s_Data.orthoSize, s_Data.orthoSize, -s_Data.orthoSize, s_Data.orthoSize, s_Data.lightNear, s_Data.lightFar);
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::DragFloat("far", &s_Data.lightFar, 0.1f, 100.0f, 10000.0f)) {
				//s_Data.lightProj = glm::perspective(45.0f, 1.0f, s_Data.lightNear, s_Data.lightFar);
				s_Data.lightProj = glm::ortho(-s_Data.orthoSize, s_Data.orthoSize, -s_Data.orthoSize, s_Data.orthoSize, s_Data.lightNear, s_Data.lightFar);
			}
			ImGui::PopItemWidth();

			ImGui::Separator();
			std::string label = "shader";
			static Ref<Shader> selectedShader;
			if (selectedShader) {
				label = selectedShader->GetName();
			}
			static int item_current_idx = 0;
			static int index = 0;
			if (ImGui::BeginCombo("##Shaders", label.c_str()))
			{
				for (auto& shader : s_Data.shaders.GetShaders())
				{
					//const bool is_selected = (item_current_idx == n);
					if (ImGui::Selectable(shader.first.c_str(), true)) {
						selectedShader = shader.second;
					}

					ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload shader")) {
				Reload(selectedShader);
			}
			ImGui::Separator();
			ImGui::End();
		}
		//Environment settings
		if (*environmentOpen) {
			ImGui::Begin(ICON_FA_TREE" Environment", environmentOpen);
			if (ImGui::Button("HDR", { 40,30 })) {
				auto path = FileDialogs::OpenFile("HDR (*.hdr)\0*.hdr\0");
				if (path) {
					//Add texture as sRGB color space if it is binded to 0 (diffuse texture binding)
					s_Data.environment = CreateRef<Environment>(Texture2D::CreateHDR(*path, false, true));
					s_Data.scene->m_EnvironmentPath = *path;
				}
			}
			if (s_Data.environment) {
				ImGui::Image((ImTextureID)s_Data.environment->GetBackgroundTextureID(), { 300, 150 }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			}
			if (ImGui::DragFloat("Intensity", &s_Data.intensity, 0.01f, 1, 20)) {
				if (s_Data.environment) {
					s_Data.environment->SetIntensity(s_Data.intensity);
				}
			}
			ImGui::End();
		}
	}

	void SceneRenderer::SetScene(const Ref<Scene>& scene)
	{
		s_Data.scene = scene;
		auto path = scene->m_EnvironmentPath;
		if (s_Data.environment) {
			s_Data.scene->m_EnvironmentPath = s_Data.environment->GetPath();
		}
		if (!path.empty()) {
			s_Data.environment = CreateRef<Environment>(Texture2D::CreateHDR(path, false, true));
		}
	}

	uint32_t SceneRenderer::GetTextureID(int index)
	{
		if (s_Data.useFxaa) {
			return s_Data.aaPass->GetSpecification().TargetFrameBuffer->GetColorAttachmentRendererID(index);
		}
		else
		{
			return s_Data.lightingPass->GetSpecification().TargetFrameBuffer->GetColorAttachmentRendererID(index);
		}
	}

	Syndra::FramebufferSpecification SceneRenderer::GetMainFrameSpec()
	{
		return s_Data.geoPass->GetSpecification().TargetFrameBuffer->GetSpecification();
	}

	Syndra::Ref<Syndra::FrameBuffer> SceneRenderer::GetGeoFrameBuffer()
	{
		return s_Data.geoPass->GetSpecification().TargetFrameBuffer;
	}

	Syndra::ShaderLibrary& SceneRenderer::GetShaderLibrary()
	{
		return s_Data.shaders;
	}

}
