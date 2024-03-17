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

#include <iostream>
#include <algorithm>

EditorGUI::EditorGUI()
{
	_assetFileTree = new AssetFileTree;
	_sceneGraphEditor = new SceneGraphEditor;
    _loggerWindow = new LoggerGUI;
    _inspector = new Inspector;
}

void EditorGUI::init(GLFWwindow* window, std::shared_ptr<DeviceExtended> device)
{
	backendDevice = device;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window, true);

	createVulkanResource();

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = backendDevice->_instance;
	init_info.PhysicalDevice = backendDevice->physical_device;
	init_info.Device = backendDevice->device;
	init_info.Queue = backendDevice->get_queue(vkb::QueueType::graphics).value();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = descriptorPool;
	init_info.MinImageCount = MAX_FRAME_IN_FLIGHT;
	init_info.ImageCount = MAX_FRAME_IN_FLIGHT;
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

	_assetFileTree->init(device.get());
	_sceneGraphEditor->init();
	_sceneGraphEditor->setOpen();
    _loggerWindow->init();
	const auto dialogFlags = ImGuiFileDialogFlags_DisableThumbnailMode | ImGuiFileDialogFlags_DontShowHiddenFiles | ImGuiFileDialogFlags_Modal;
	ImGuiFileDialog::Instance()->OpenDialog("ChoosePBRTFileDlgKey", "Choose .pbrt file", ".pbrt", ".", 1, nullptr, dialogFlags);
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
				auto* sceneGraph = _sceneGraphEditor->parsePBRTSceneFile(fsPath, _assetFileTree->assetManager);
                if(viewer!= nullptr) viewer->setCurrentSceneGraph(sceneGraph,_assetFileTree->assetManager);
				fileSelectorOpen = false;
			}
		}
	}

	ImGui::Render();
}

vk::CommandBuffer EditorGUI::recordGraphicsCommand(unsigned int idx)
{
	vk::CommandBuffer cmd = commandBuffers[idx];
	render(cmd, idx);
	return cmd;
}

void EditorGUI::render(vk::CommandBuffer cmdBuf, unsigned int idx)
{
	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmdBuf.begin(beginInfo);

	vk::RenderPassBeginInfo renderPassBegInfo{};
	renderPassBegInfo.setRenderPass(guiPass);
	renderPassBegInfo.setFramebuffer(frameBuffers[idx]);
	renderPassBegInfo.renderArea.extent.width = backendDevice->_swapchain.extent.width;
	renderPassBegInfo.renderArea.extent.height = backendDevice->_swapchain.extent.height;
//	renderPassBegInfo.setClearValueCount(1);
//	auto clearValue = vk::ClearValue{};
//	clearValue.setColor(vk::ClearColorValue{});
//	renderPassBegInfo.setClearValues(clearValue);
	cmdBuf.beginRenderPass(renderPassBegInfo,vk::SubpassContents::eInline);//todo

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

	cmdBuf.endRenderPass();
	cmdBuf.end();
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
	uint32_t graphicsQueueFamilyIdx = backendDevice->get_queue_index(vkb::QueueType::graphics).value();
	vk::CommandPoolCreateInfo poolInfo{};
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIdx;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
	{
		commandPools.emplace_back(backendDevice,backendDevice->createCommandPool(poolInfo));
	}

	for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
	{
		commandBuffers.push_back(commandPools[i].allocateCommandBuffer(vk::CommandBufferLevel::ePrimary));
	}

	//create render pass
	
	createRenderPass(&backendDevice->_swapchain);
	backendDevice->_swapchain.registerRecreateCallback([this](SwapchainExtended* swapchain) {
		createRenderPass(swapchain);
	});
	//create frame buffer
	createGuiFrameBuffer(&backendDevice->_swapchain);
	backendDevice->_swapchain.registerRecreateCallback([this](SwapchainExtended* swapchain) {
		createGuiFrameBuffer(swapchain);
	});

	//create descriptor pool
	vk::DescriptorPoolSize pool_sizes[] = {
		//{vk::DescriptorType::eSampler, MAX_FRAME_IN_FLIGHT},
		{vk::DescriptorType::eCombinedImageSampler, MAX_FRAME_IN_FLIGHT},
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
	poolCreateInfo.maxSets = MAX_FRAME_IN_FLIGHT;
	poolCreateInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
	poolCreateInfo.pPoolSizes = pool_sizes;
	descriptorPool = backendDevice->createDescriptorPool(poolCreateInfo);
}

void EditorGUI::createRenderPass(SwapchainExtended* swapchain)
{
	if (guiPass != VK_NULL_HANDLE)
	{
		backendDevice->destroyRenderPass(guiPass);
	}

	vk::AttachmentDescription attachDesc = {};
	attachDesc.format = vk::Format(swapchain->image_format);
	attachDesc.samples = vk::SampleCountFlagBits::e1;
	attachDesc.loadOp = vk::AttachmentLoadOp::eLoad;
	attachDesc.storeOp = vk::AttachmentStoreOp::eStore;
	attachDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachDesc.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachDesc.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference colorAttach = {};
	colorAttach.attachment = 0;
	colorAttach.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpassDesc.colorAttachmentCount = 1,
	subpassDesc.pColorAttachments = &colorAttach;

	vk::SubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = vk::AccessFlagBits::eNone;
	dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

	vk::RenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachDesc;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	guiPass = backendDevice->createRenderPass(renderPassInfo);
}

void EditorGUI::createGuiFrameBuffer(SwapchainExtended* swapchain)
{
	auto swapChainImgViewsCount = swapchain->image_count;
	if (!frameBuffers.empty()) {
		for (auto& framebuffer : frameBuffers) {
			backendDevice->destroyFramebuffer(framebuffer);
		}
	}

	frameBuffers.clear();
	frameBuffers.resize(swapChainImgViewsCount);

	int width = swapchain->extent.width;
	int height = swapchain->extent.height;

	for (uint32_t i = 0; i < swapChainImgViewsCount; i++)
	{
		vk::ImageView attachments =  swapchain->get_image_views().value()[i];
		vk::FramebufferCreateInfo frameBufferInfo = {};
		frameBufferInfo.renderPass = guiPass;
		frameBufferInfo.attachmentCount = 1;
		frameBufferInfo.pAttachments = &attachments;
		frameBufferInfo.width = width;
		frameBufferInfo.height = height;
		frameBufferInfo.layers = 1;

		frameBuffers[i] = backendDevice->createFramebuffer(frameBufferInfo);
	}
}

void EditorGUI::destroyVulkanResource()
{

}

