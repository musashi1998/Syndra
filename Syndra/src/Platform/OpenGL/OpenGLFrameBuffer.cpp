#include "lpch.h"
#include "Platform/OpenGL/OpenGLFrameBuffer.h"
#include <glad/glad.h>

namespace Syndra {

	static const uint32_t s_MaxFramebufferSize = 8192;

	OpenGLFrameBuffer::OpenGLFrameBuffer(const FramebufferSpecification& spec)
		:m_Specification(spec)
	{
		Invalidate();
	}

	OpenGLFrameBuffer::~OpenGLFrameBuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		glDeleteTextures(1, &m_DepthAttachment);
	}

	void OpenGLFrameBuffer::Invalidate()
	{
		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
			glDeleteTextures(1, &m_ColorAttachment);
			glDeleteTextures(1, &m_DepthAttachment);
		}

		glCreateFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

		if (m_Specification.Samples == 1) {
			glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
			glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Specification.Width, m_Specification.Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(GL_TEXTURE_2D, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

			glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
			glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height);
			glBindTexture(GL_TEXTURE_2D, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
		}
		else 
		{
			glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &m_ColorAttachment);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_ColorAttachment);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,m_Specification.Samples, GL_RGB, m_Specification.Width, m_Specification.Height, GL_TRUE);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_ColorAttachment, 0);
		
			glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &m_DepthAttachment);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_DepthAttachment);
			glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,m_Specification.Samples , GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height,GL_TRUE);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, m_DepthAttachment, 0);
		}
		SN_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFrameBuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void OpenGLFrameBuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFrameBuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		{
			SN_CORE_ERROR("Attempted to rezize framebuffer to {0}, {1}", width, height);
			return;
		}
		m_Specification.Width = width;
		m_Specification.Height = height;
		Invalidate();
	}

}