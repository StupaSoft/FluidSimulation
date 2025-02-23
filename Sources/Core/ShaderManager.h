#pragma once

#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <thread>
#include <future>
#include <tuple>

#include "VulkanCore.h"
#include "VulkanUtility.h"

class ShaderManager
{
private:
	std::mutex _mtx;
	std::map<std::filesystem::path, std::filesystem::path> _shaderArchive;

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

	VkShaderModule GetShaderModule(const std::string &shaderStem);

private:
	ShaderManager();

	void CompileAllShaders();
	bool CompileShader(const std::filesystem::path &shaderPath, const std::filesystem::path &outPath);

	void ScanAllShaders();
};