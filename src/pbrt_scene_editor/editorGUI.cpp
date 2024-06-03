#include "editorGUI.h"

#include "VulkanExtension.h"

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

#include "ImGuiFileDialog.h"

#include "Inspector.hpp"
#include "AssetFileTree.hpp"
#include "sceneViewer.hpp"
#include "sceneGraphEditor.hpp"
#include "LoggerGUI.hpp"
#include "offlineRender.hpp"

#include "FrameGraph.hpp"
#include "PassDefinition.h"

#include <iostream>
#include <algorithm>

EditorGUI::EditorGUI()
{
	_assetFileTree = new AssetFileTree;
	_sceneGraphEditor = new SceneGraphEditor;
    _loggerWindow = new LoggerGUI;
    _inspector = new Inspector;
	_offlineRender = new OfflineRenderGUI;
}

void EditorGUI::init(GLFWwindow* window, std::shared_ptr<DeviceExtended> device)
{
	backendDevice = device;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui_ImplGlfw_InitForVulkan(window, true);

	ImGui::StyleColorsDark();

	_assetFileTree->init(device.get());
	_sceneGraphEditor->init();
	_sceneGraphEditor->setOpen();
    _loggerWindow->init();
	const auto dialogFlags = ImGuiFileDialogFlags_DisableThumbnailMode | ImGuiFileDialogFlags_DontShowHiddenFiles | ImGuiFileDialogFlags_Modal;
	ImGuiFileDialog::Instance()->OpenDialog("ChoosePBRTFileDlgKey", "Choose .pbrt file", ".pbrt", ".", 1, nullptr, dialogFlags);
}

void EditorGUI::renderInit(vk::RenderPass guiPass)
{
	createVulkanResource();

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = backendDevice->_instance;
	init_info.PhysicalDevice = backendDevice->physical_device;
	init_info.Device = backendDevice->device;
	init_info.Queue = backendDevice->get_queue(vkb::QueueType::graphics).value();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = descriptorPool;
	init_info.MinImageCount = NUM_MIN_SWAPCHAIN_IMAGE;
	init_info.ImageCount = NUM_MIN_SWAPCHAIN_IMAGE;
	init_info.Allocator = VK_NULL_HANDLE;
	init_info.CheckVkResultFn = nullptr;

	ImGui_ImplVulkan_Init(&init_info, guiPass);

	vk::CommandBuffer singleUseCommand = backendDevice->allocateOnceGraphicsCommand();
	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	singleUseCommand.begin(beginInfo);
	ImGui_ImplVulkan_CreateFontsTexture(singleUseCommand);
	singleUseCommand.end();
	auto wait_and_free = backendDevice->submitOnceGraphicsCommand(singleUseCommand);
	wait_and_free();
}

void EditorGUI::showMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			showMenuFile();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			showMenuEdit();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Shading"))
		{
			showMenuShading();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			showMenuView();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Render"))
		{
			showMenuRender();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools"))
		{
			showMenuTools();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void EditorGUI::showMenuFile()
{
	if (ImGui::MenuItem("New")) {
		//printf("Click New\n");
	}
	if (ImGui::MenuItem("Open", "Ctrl+O")) {
		fileSelectorOpen = true;
	}
	
	if (ImGui::BeginMenu("Open Recent"))
	{
		//todo : BUG : can't remapped scene file
		if (!recentOpenCache.empty()) {
			int have_listed = 0;
			int size = recentOpenCache.size();
			int idx =  size - 1;

			for (; idx >= 0 && have_listed < 3; idx--)
			{
				auto& histroy = recentOpenCache[idx];
				if (ImGui::MenuItem(histroy.first.c_str())) {
					// todo : repush hit histroy
					_sceneGraphEditor->parsePBRTSceneFile(histroy.second, _assetFileTree->assetManager);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s",histroy.second.string().c_str());
				have_listed++;
			}

			if (idx >= 0) {
				if (ImGui::BeginMenu("More.."))
				{
					for (; idx >= 0; idx--)
					{
						auto& histroy = recentOpenCache[idx];
						if (ImGui::MenuItem(histroy.first.c_str())) {
							//todo : re-push hit history
							_sceneGraphEditor->parsePBRTSceneFile(histroy.second, _assetFileTree->assetManager);
						}
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s",histroy.second.string().c_str());
					}
					ImGui::EndMenu();
				}
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Save", "Ctrl+S")) {}
	if (ImGui::MenuItem("Save As..")) {}

	if (ImGui::BeginMenu("Import"))
	{
		ImGui::MenuItem(".obj");
		ImGui::MenuItem(".fbx");
		ImGui::MenuItem(".gltf");
		if (ImGui::BeginMenu("More.."))
		{
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

	ImGui::Separator();
	if (ImGui::BeginMenu("Options"))
	{
		static bool enabled = true;
		ImGui::MenuItem("Enabled", "", &enabled);
		ImGui::BeginChild("child", ImVec2(0, 60), true);
		for (int i = 0; i < 10; i++)
			ImGui::Text("Scrolling Text %d", i);
		ImGui::EndChild();
		static float f = 0.5f;
		static int n = 0;
		ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
		ImGui::InputFloat("Input", &f, 0.1f);
		ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Colors"))
	{
		float sz = ImGui::GetTextLineHeight();
		for (int i = 0; i < ImGuiCol_COUNT; i++)
		{
			const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
			ImGui::Dummy(ImVec2(sz, sz));
			ImGui::SameLine();
			ImGui::MenuItem(name);
		}
		ImGui::EndMenu();
	}

	// Here we demonstrate appending again to the "Options" menu (which we already created above)
	// Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
	// In a real code-base using it would make senses to use this feature from very different code locations.
	if (ImGui::BeginMenu("Options")) // <-- Append!
	{
		static bool b = true;
		ImGui::Checkbox("SomeOption", &b);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Disabled", false)) // Disabled
	{
		IM_ASSERT(0);
	}
	if (ImGui::MenuItem("Checked", NULL, true)) {}
	if (ImGui::MenuItem("Quit", "Alt+F4")) {}
}

void EditorGUI::showMenuEdit()
{
	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
	ImGui::Separator();
	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
}

void EditorGUI::showMenuShading()
{
	if (ImGui::BeginMenu("Effects")) // <-- Append!
	{
		ImGui::Checkbox("WireFrame", &viewer->enableWireFrame);
		static bool enableAO;
		if (ImGui::BeginMenu("AO"))
		{
			static int ao = 0;
			ImGui::RadioButton("Disable", &ao, 0);
			ImGui::RadioButton("Screen Space AO", &ao, 1);
			ImGui::RadioButton("Ray Trace AO", &ao, 2);
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}
	static int e = 4;
	ImGui::RadioButton("Flat", &e, 0);
	ImGui::RadioButton("MeshID", &e, 1);
	ImGui::RadioButton("World Position", &e, 2);
	ImGui::RadioButton("World Normal", &e, 3);
	ImGui::RadioButton("UV", &e, 4);
	ImGui::RadioButton("Albedo", &e, 5);
	ImGui::RadioButton("Final", &e, 6);

	switch (e)
	{
	case 0:
		viewer->currenShadingMode = SceneViewer::ShadingMode::FLAT;
		break;
    case 1:
        viewer->currenShadingMode = SceneViewer::ShadingMode::MESHID;
        break;
	case 2:
		viewer->currenShadingMode = SceneViewer::ShadingMode::POSITION;
		break;
	case 3:
		viewer->currenShadingMode = SceneViewer::ShadingMode::NORMAL;
		break;
	case 4:
		viewer->currenShadingMode = SceneViewer::ShadingMode::UV;
		break;
	case 5:
		viewer->currenShadingMode = SceneViewer::ShadingMode::ALBEDO;
		break;
	case 6:
		viewer->currenShadingMode = SceneViewer::ShadingMode::FINAL;
		break;
	default:
		break;
	}
}

void EditorGUI::showMenuView()
{
	if (ImGui::MenuItem("SceneGraphEditor")) {
		_sceneGraphEditor->setOpen();
	}
	if (ImGui::MenuItem("Asset")) {
		_assetFileTree->setOpen();
	}
	if (ImGui::MenuItem("Inspector")) {
		_inspector->setOpen();
	}
	if (ImGui::MenuItem("Log")) {
		_loggerWindow->setOpen();
	}
}

void EditorGUI::showMenuRender()
{
	if (ImGui::MenuItem("Render Current Frame")) {
		_offlineRender->render(currentPBRTSceneFilePath.make_preferred().string());
	}

	if (ImGui::MenuItem("Render Interactive Session")) {

	}

	if (ImGui::BeginMenu("Bake Lighting")) {
		static bool b_all = false;
		ImGui::Checkbox("All", &b_all);

		static bool b_diffuse = true;
		b_diffuse |= b_all;
		ImGui::Checkbox("Diffuse", &b_diffuse);

		ImGui::EndMenu();
	}
}

void EditorGUI::showMenuTools()
{

}

void EditorGUI::constructFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	showMainMenuBar();
    _inspector->constructFrame();
	_assetFileTree->constructFrame();
	_sceneGraphEditor->constructFrame();
    _loggerWindow->constructFrame();

	if (fileSelectorOpen) {
		
		if (ImGuiFileDialog::Instance()->Display("ChoosePBRTFileDlgKey"))
		{
			if (ImGuiFileDialog::Instance()->IsOk())
			{
				auto fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
				auto filePath= ImGuiFileDialog::Instance()->GetCurrentPath();
				std::filesystem::path fsPath(filePath);
				fsPath.append(fileName);
				if (true) { // if load sucess
					auto fileHistroy = std::pair{ fileName,fsPath };
					recentOpenCache.erase(std::remove(recentOpenCache.begin(),recentOpenCache.end(),fileHistroy),recentOpenCache.end());
					recentOpenCache.push_back(fileHistroy);
					if (recentOpenCache.size() > 10) {
						recentOpenCache.pop_front();
					}
				}
				currentPBRTSceneFilePath = fsPath;
				auto* sceneGraph = _sceneGraphEditor->parsePBRTSceneFile(fsPath, _assetFileTree->assetManager);
                if(viewer!= nullptr) viewer->setCurrentSceneGraph(sceneGraph,_assetFileTree->assetManager);
				fileSelectorOpen = false;
			}
		}
	}

	ImGui::Render();
}

void EditorGUI::render(vk::CommandBuffer cmdBuf)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
}

EditorGUI::~EditorGUI()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
	destroyVulkanResource();
	if(_inspector!=nullptr)
		delete _inspector;
	if(_assetFileTree!=nullptr)
		delete _assetFileTree;
	if(_sceneGraphEditor!=nullptr)
		delete _sceneGraphEditor;
}

void EditorGUI::createVulkanResource()
{
	//create descriptor pool
	vk::DescriptorPoolSize pool_sizes[] = {
		//{vk::DescriptorType::eSampler, MAX_FRAME_IN_FLIGHT},
		{vk::DescriptorType::eCombinedImageSampler, NUM_MIN_SWAPCHAIN_IMAGE},
		//{vk::DescriptorType::eSampledImage, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eStorageImage, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eUniformTexelBuffer, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eStorageTexelBuffer, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eUniformBuffer, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eStorageBuffer, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eUniformBufferDynamic, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eStorageBufferDynamic, MAX_FRAME_IN_FLIGHT},
		//{vk::DescriptorType::eInputAttachment, MAX_FRAME_IN_FLIGHT},
	};

	vk::DescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.maxSets = NUM_MIN_SWAPCHAIN_IMAGE;
	poolCreateInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
	poolCreateInfo.pPoolSizes = pool_sizes;
	descriptorPool = backendDevice->createDescriptorPool(poolCreateInfo);
}

void EditorGUI::destroyVulkanResource()
{

}

void EditorGUI::constructFrameGraphAOT(FrameGraph* frameGraph)
{
	auto guiPass = std::make_unique<GUIPass>();
	guiPass->gui = this;
	
	guiPass->renderTo(frameGraph->getPresentTexture(), vk::AttachmentLoadOp::eLoad);

	frameGraph->executePass(std::move(guiPass));
}

