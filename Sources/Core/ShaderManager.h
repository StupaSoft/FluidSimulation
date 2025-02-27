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
#include "ShaderResource.h"

class ShaderManager
{
private:
	std::map<std::tuple<std::filesystem::path, std::string>, ShaderAsset> _shaderArchive; // (path, entry name) -> Shader asset

	// Slang
	Slang::ComPtr<slang::IGlobalSession> _globalSession;
	Slang::ComPtr<slang::ISession> _session;

public:
	static ShaderManager *Get()
	{
		static std::unique_ptr<ShaderManager> shaderManager(new ShaderManager());
		return shaderManager.get();
	}

	ShaderManager(const ShaderManager &other) = delete;
	ShaderManager &operator=(const ShaderManager &other) = delete;
	ShaderManager(ShaderManager &&other) = delete;
	ShaderManager &operator=(ShaderManager &&other) = delete;
	~ShaderManager() = default;

	ShaderAsset GetShaderAsset(const std::string &shaderStem, const std::string &entryName = "main");

private:
	ShaderManager();
	Slang::ComPtr<slang::IComponentType> CompileShader(const std::filesystem::path &shaderPath, const std::string &entryName);
};