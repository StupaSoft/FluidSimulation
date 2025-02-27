#include "ShaderResource.h"

ShaderResource::ShaderResource(const Slang::ComPtr<slang::IComponentType> &program) :
	_program(program)
{
	// Now we will create a shader module from program
	// Linking
	Slang::ComPtr<slang::IComponentType> linkedProgram;
	Slang::ComPtr<ISlangBlob> diagnosticBlob;
	_program->link(linkedProgram.writeRef(), diagnosticBlob.writeRef());

	// Get a kernel code
	uint32_t entryPointIndex = 0;
	uint32_t targetIndex = 0;
	Slang::ComPtr<slang::IBlob> kernelBlob;
	Slang::ComPtr<slang::IBlob> diagnostics;
	linkedProgram->getEntryPointCode(entryPointIndex, targetIndex, kernelBlob.writeRef(), diagnostics.writeRef());

	// Convert to a vector of char
	const char *codePtr = reinterpret_cast<const char *>(kernelBlob->getBufferPointer());
	std::vector<char> code(codePtr, codePtr + kernelBlob->getBufferSize());

	VkShaderModuleCreateInfo createInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = kernelBlob->getBufferSize(),
		.pCode = reinterpret_cast<const uint32_t *>(kernelBlob->getBufferPointer())
	};

	if (vkCreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), &createInfo, nullptr, &_shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module.");
	}
}

ShaderResource::~ShaderResource()
{
	//vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), _shaderModule, nullptr);
}
