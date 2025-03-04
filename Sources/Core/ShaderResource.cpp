#include "ShaderResource.h"

ShaderAsset::ShaderAsset(const Slang::ComPtr<slang::IComponentType> &program) :
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

	// Extract and store the (variable, binding) pair with the reflection API
	auto programLayout = _program->getLayout();
	auto globalTypeLayout = programLayout->getGlobalParamsTypeLayout();
	auto paramCount = globalTypeLayout->getFieldCount();
	for (int i = 0; i < paramCount; i++)
	{
		auto globalParam = globalTypeLayout->getFieldByIndex(i);
		_paramToBinding[globalParam->getName()] = globalParam->getBindingIndex();
	}

	// Get the stage
	_shaderStage |= SlangStageToFlagBit(programLayout->getEntryPointByIndex(0)->getStage());
}

uint32_t ShaderAsset::GetBindingIndex(const std::string &variable)
{
	if (_paramToBinding.find(variable) == _paramToBinding.end())
	{
		return -1;
	}
	else
	{
		return _paramToBinding.at(variable);
	}
	
}

VkShaderStageFlagBits ShaderAsset::SlangStageToFlagBit(SlangStage slangStage)
{
	switch (slangStage)
	{
	case SLANG_STAGE_VERTEX:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SLANG_STAGE_FRAGMENT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SLANG_STAGE_COMPUTE:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	default:
		return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	}
}

ShaderAsset::~ShaderAsset()
{
	vkDestroyShaderModule(VulkanCore::Get()->GetLogicalDevice(), _shaderModule, nullptr);
}
