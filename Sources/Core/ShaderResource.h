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

class ShaderAsset;
using Shader = std::shared_ptr<ShaderAsset>;

inline Shader CreateShaderAsset(const Slang::ComPtr<slang::IComponentType> &program) { return std::make_shared<ShaderAsset>(program); }

class ShaderAsset
{
private:
	Slang::ComPtr<slang::IComponentType> _program; // For reflection
	VkShaderModule _shaderModule = VK_NULL_HANDLE; // SPIR-V module
	VkShaderStageFlags _shaderStage = 0;
	std::map<std::string, uint32_t> _paramToBinding; // parameter Name -> binding

public:
	ShaderAsset(const Slang::ComPtr<slang::IComponentType> &program);
	~ShaderAsset();

	uint32_t GetBindingIndex(const std::string &variable);
	VkShaderModule GetShaderModule() { return _shaderModule; }
	VkShaderStageFlags GetShaderStage() { return _shaderStage; }

private:
	VkShaderStageFlagBits SlangStageToFlagBit(SlangStage slangStage);
};