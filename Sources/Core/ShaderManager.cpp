#include "ShaderManager.h"

ShaderManager::ShaderManager()
{
	CompileAllShaders();
	ScanAllShaders();
}

VkShaderModule ShaderManager::GetShaderModule(const std::string &shaderStem)
{
	if (_shaderArchive.find(shaderStem) == _shaderArchive.end())
	{
		throw std::runtime_error(std::format("Shader not found: {}", shaderStem));
	}

	auto code = ReadFile(_shaderArchive.at(shaderStem).string());

	// std::vector<char> &code;
	VkShaderModuleCreateInfo createInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t *>(code.data())
	};

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(VulkanCore::Get()->GetLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a shader module.");
	}

	return shaderModule;
}

void ShaderManager::CompileAllShaders()
{
	auto sourceDir = std::filesystem::path(SHADER_DIR) / "Sources";
	auto outDir = std::filesystem::path(SHADER_DIR) / "Out";

	std::vector<std::tuple<std::filesystem::path, std::future<bool>>> compileTasks;

	// Recursively scan the source directory and compile shaders selectively
	for (const auto &entry : std::filesystem::recursive_directory_iterator(sourceDir))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".slang")
		{
			auto shaderPath = entry.path();

			// Is a Slang file
			std::filesystem::path SPIRVName = entry.path().filename();
			SPIRVName.replace_extension(".spv");

			// Check if a precompiled SPIRV exists
			bool needCompilation = false;
			auto SPIRVPath = outDir / SPIRVName;
			if (std::filesystem::exists(SPIRVPath))
			{
				// If the compiled SPIR-V has been outdated, compile the shader source once again.
				auto lastEditTime = std::filesystem::last_write_time(shaderPath);
				auto lastCompileTime = std::filesystem::last_write_time(SPIRVPath);
				needCompilation = lastEditTime > lastCompileTime;
			}
			else
			{
				// If a compiled SPIR-V file doesn't exist, compile the shader source.
				needCompilation = true;
			}

			std::string initMessage;
			std::filesystem::path shaderStem = shaderPath.filename().stem();
			if (needCompilation)
			{
				// Asynchronously launch a compilation task
				std::future<bool> compileTask = std::async(std::launch::async, [this, &shaderPath, &SPIRVPath]() { return CompileShader(shaderPath, SPIRVPath); });
				compileTasks.push_back(std::make_tuple(SPIRVPath, std::move(compileTask)));

				initMessage = std::format("Compiling: {}", shaderStem.string());
			}
			else
			{
				initMessage = std::format("Shader found: {}", shaderStem.string());
			}

			std::cout << initMessage << std::endl;
		}
	}

	for (auto &taskPair : compileTasks)
	{
		auto &SPIRVPath = std::get<0>(taskPair);
		auto shaderStem = SPIRVPath.filename().stem();
		auto &task = std::get<1>(taskPair);

		bool isSuccess = task.get();
		if (isSuccess)
		{
			std::cout << std::format("Successfully compiled: {}", shaderStem.string());
		}
		else
		{
			std::cout << std::format("Compilation failed: {}", shaderStem.string());
		}
	}
}

bool ShaderManager::CompileShader(const std::filesystem::path &shaderPath, const std::filesystem::path &outPath)
{
	return true;
}

void ShaderManager::ScanAllShaders()
{
	auto outDir = std::filesystem::path(SHADER_DIR) / "Out";
	for (const auto &entry : std::filesystem::directory_iterator(outDir))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".spv")
		{
			auto SPIRVPath = entry.path();
			auto shaderStem = SPIRVPath.filename().stem();

			_shaderArchive[shaderStem] = SPIRVPath;
		}
	}
}
