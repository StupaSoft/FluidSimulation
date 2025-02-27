#include "ShaderManager.h"

ShaderManager::ShaderManager()
{
	// Setup Slang environment
	SlangGlobalSessionDesc desc{};
	createGlobalSession(&desc, _globalSession.writeRef());

	slang::TargetDesc targetDesc
	{
		.format = SLANG_SPIRV,
		.profile = _globalSession->findProfile("sm_6_1")
	};

	slang::SessionDesc sessionDesc
	{
		.targets = &targetDesc,
		.targetCount = 1,
		.defaultMatrixLayoutMode = SlangMatrixLayoutMode::SLANG_MATRIX_LAYOUT_COLUMN_MAJOR
	};

	_globalSession->createSession(sessionDesc, _session.writeRef());
}

ShaderAsset ShaderManager::GetShaderAsset(const std::string &shaderStem, const std::string &entryName)
{
	auto shaderPair = std::make_tuple(shaderStem, entryName);
	if (_shaderArchive.find(shaderPair) == _shaderArchive.end())
	{
		// Cached shader not found yet - try to compile
		std::filesystem::path shaderName(shaderStem);
		shaderName.replace_extension(".slang");

		std::filesystem::path shaderPath;
		for (const auto &entry : std::filesystem::recursive_directory_iterator(SHADER_DIR))
		{
			if (entry.is_regular_file() && entry.path().filename() == shaderName)
			{
				shaderPath = entry.path();
			}
		}

		if (shaderPath.empty())
		{
			throw std::runtime_error(std::format("Shader does not exist: {}", shaderStem));
		}

		auto compiledShader = CompileShader(shaderPath, entryName);
		if (compiledShader == nullptr)
		{
			throw std::runtime_error(std::format("Shader compilation failed: ({} | {})", shaderStem, entryName));
		}
		else
		{
			std::cout << std::format("Successfully compiled: ({} | {})", shaderStem, entryName) << std::endl;
			_shaderArchive[shaderPair] = CreateShaderAsset(compiledShader);
		}
	}

	return _shaderArchive[shaderPair];
}

Slang::ComPtr<slang::IComponentType> ShaderManager::CompileShader(const std::filesystem::path &shaderPath, const std::string &entryName)
{
	// Search paths for #include directive or import declaration

	// Pre-defined macros

	// // Load a module and capture diagnostic output
	Slang::ComPtr<slang::IBlob> diagnostics;
	slang::IModule *module = _session->loadModule(shaderPath.string().c_str(), diagnostics.writeRef());

	if (diagnostics)
	{
		std::cout << reinterpret_cast<const char *>(diagnostics->getBufferPointer()) << std::endl;
		return nullptr;
	}

	// Find an entry point
	Slang::ComPtr<slang::IEntryPoint> entryPoint;
	module->findEntryPointByName(entryName.c_str(), entryPoint.writeRef());

	// Composition
	std::vector<slang::IComponentType*> components = { module, entryPoint };
	Slang::ComPtr<slang::IComponentType> program;
	_session->createCompositeComponentType(components.data(), components.size(), program.writeRef());

	if (program == nullptr)
	{
		std::cout << std::format("Invalid shader program: {}", shaderPath.string()) << std::endl;
		return nullptr;
	}

	return program;
}