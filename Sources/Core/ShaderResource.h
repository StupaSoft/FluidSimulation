#pragma once

#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <thread>
#include <future>
#include <tuple>
#include <exception>

#include "slang.h"
#include "slang-com-ptr.h"

#include "VulkanCore.h"
#include "VulkanUtility.h"

class ShaderResource;
using ShaderAsset = std::shared_ptr<ShaderResource>;

inline ShaderAsset CreateShaderAsset(const Slang::ComPtr<slang::IComponentType> &program) { return std::make_shared<ShaderResource>(program); }

class ShaderResource
{
private:
	Slang::ComPtr<slang::IComponentType> _program; // For reflection
	VkShaderModule _shaderModule = VK_NULL_HANDLE; // SPIR-V module
	VkShaderStageFlags _shaderStage = 0;
	std::map<std::string, uint32_t> _paramToBinding; // parameter Name -> binding

public:
	ShaderResource(const Slang::ComPtr<slang::IComponentType> &program);
	~ShaderResource();

	uint32_t GetBindingIndex(const std::string &variable);
	VkShaderModule GetShaderModule() { return _shaderModule; }
	VkShaderStageFlags GetShaderStage() { return _shaderStage; }

private:
	VkShaderStageFlagBits SlangStageToFlagBit(SlangStage slangStage);
};