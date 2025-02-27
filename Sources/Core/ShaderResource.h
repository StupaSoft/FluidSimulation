#pragma once

#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <thread>
#include <future>
#include <tuple>

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

public:
	ShaderResource(const Slang::ComPtr<slang::IComponentType> &program);
	~ShaderResource();

	Slang::ComPtr<slang::IComponentType> GetProgram() { return _program; }
	VkShaderModule GetShaderModule() { return _shaderModule; }
};