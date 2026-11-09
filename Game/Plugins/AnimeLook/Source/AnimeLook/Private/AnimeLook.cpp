// The anime look as one full-screen pass, right before Unreal's post processing.
//
// It ignores Unreal's lighting on purpose and shades every lit pixel again from the G-buffer:
// albedo times one of two tones, decided by a hard edge on N.L with a light that always sits
// up and behind the camera (so faces never end up in the dark, the Guilty Gear trick). On top
// come ink lines where depth or normals jump, a painted sky gradient and the impact frame.
//
// Everything is driven by console variables, so the game can talk to it without depending on
// this plugin, and the whole thing can be switched off with r.AnimeLook 0.

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneRenderTargetParameters.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ShaderCore.h"
#include "ShaderParameterStruct.h"

namespace AnimeLook
{
	TAutoConsoleVariable<int32> CVarEnabled(TEXT("r.AnimeLook"), 1,
		TEXT("Anime look: toon shading, ink lines, painted sky. 0 shows the plain render."), ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarLineWidth(TEXT("r.AnimeLook.LineWidth"), 1.5f,
		TEXT("Ink line width in pixels at 1080p."), ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarShadowEdge(TEXT("r.AnimeLook.ShadowEdge"), 0.1f,
		TEXT("Where light turns into shadow, as N.L. Higher means more shadow."), ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarImpact(TEXT("r.AnimeLook.Impact"), 0.f,
		TEXT("Impact frame strength, 0..1. Set by the game on big hits."), ECVF_RenderThreadSafe);

	class FAnimeLookPS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FAnimeLookPS);
		SHADER_USE_PARAMETER_STRUCT(FAnimeLookPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
			SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
			SHADER_PARAMETER(FVector3f, ShadowTint)
			SHADER_PARAMETER(FVector3f, InkColor)
			SHADER_PARAMETER(FVector3f, SkyTop)
			SHADER_PARAMETER(FVector3f, SkyBottom)
			SHADER_PARAMETER(float, LineWidth)
			SHADER_PARAMETER(float, ShadowEdge)
			SHADER_PARAMETER(float, Impact)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			// needs the deferred G-buffer
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FAnimeLookPS, "/Plugin/AnimeLook/Private/AnimeLook.usf", "MainPS", SF_Pixel);

	class FExtension final : public FSceneViewExtensionBase
	{
	public:
		explicit FExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

		virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override
		{
			if (CVarEnabled.GetValueOnRenderThread() == 0 || View.GetFeatureLevel() < ERHIFeatureLevel::SM5) return;

			TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures =
				CreateSceneTextureUniformBuffer(GraphBuilder, View, ESceneTextureSetupMode::All);
			FRDGTextureRef SceneColor = SceneTextures->GetParameters()->SceneColorTexture;
			if (!SceneColor) return;

			// Scene color is read through the scene textures, so the pass writes a fresh texture
			// and that gets copied back over scene color afterwards.
			const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(SceneColor->Desc.Extent, SceneColor->Desc.Format,
				FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
			FRDGTextureRef Output = GraphBuilder.CreateTexture(Desc, TEXT("AnimeLook.Output"));

			FAnimeLookPS::FParameters* Parameters = GraphBuilder.AllocParameters<FAnimeLookPS::FParameters>();
			Parameters->View = View.ViewUniformBuffer;
			Parameters->SceneTexturesStruct = SceneTextures;
			Parameters->ShadowTint = FVector3f(0.62f, 0.56f, 0.78f); // shadows go purple, not grey
			Parameters->InkColor = FVector3f(0.035f, 0.03f, 0.05f);
			Parameters->SkyTop = FVector3f(0.32f, 0.55f, 0.95f);
			Parameters->SkyBottom = FVector3f(0.92f, 0.94f, 1.0f);
			Parameters->LineWidth = FMath::Max(0.f, CVarLineWidth.GetValueOnRenderThread());
			Parameters->ShadowEdge = CVarShadowEdge.GetValueOnRenderThread();
			Parameters->Impact = FMath::Clamp(CVarImpact.GetValueOnRenderThread(), 0.f, 1.f);
			Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::ENoAction);

			// Whole texture: with splitscreen several views share it, the shader leaves the
			// other views' pixels as they are.
			TShaderMapRef<FAnimeLookPS> PixelShader(GetGlobalShaderMap(View.GetFeatureLevel()));
			FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GetGlobalShaderMap(View.GetFeatureLevel()), RDG_EVENT_NAME("AnimeLook"),
				PixelShader, Parameters, FIntRect(FIntPoint::ZeroValue, SceneColor->Desc.Extent));
			AddCopyTexturePass(GraphBuilder, Output, SceneColor);
		}

	protected:
		virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
		{
			return CVarEnabled.GetValueOnGameThread() != 0;
		}
	};
}

class FAnimeLookModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AnimeLook"));
		if (Plugin) AddShaderSourceDirectoryMapping(TEXT("/Plugin/AnimeLook"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		// view extensions need the engine, which isn't up yet this early
		PostEngineInit = FCoreDelegates::OnPostEngineInit.AddLambda([this]
		{
			Extension = FSceneViewExtensions::NewExtension<AnimeLook::FExtension>();
		});
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInit);
		Extension.Reset();
	}

private:
	TSharedPtr<AnimeLook::FExtension, ESPMode::ThreadSafe> Extension;
	FDelegateHandle PostEngineInit;
};

IMPLEMENT_MODULE(FAnimeLookModule, AnimeLook)
