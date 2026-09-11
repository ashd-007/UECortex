#include "MCPHttpServer.h"
#include "MCPRouter.h"
#include "MCPToolRegistry.h"
#include "UECortexModule.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FMCPHttpServer::FMCPHttpServer(uint32 InPort)
	: Port(InPort)
{
}

FMCPHttpServer::~FMCPHttpServer()
{
	Stop();
}

void FMCPHttpServer::Start()
{
	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	Router = HttpModule.GetHttpRouter(Port);

	if (!Router.IsValid())
	{
		UE_LOG(LogUECortex, Error, TEXT("UECortex: Failed to get HTTP router on port %d"), Port);
		return;
	}

	Router->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FMCPHttpServer::HandlePost));

	Router->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FMCPHttpServer::HandleSSE));

	Router->BindRoute(
		FHttpPath(TEXT("/health")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FMCPHttpServer::HandleHealth));

	// OPTIONS for CORS preflight
	Router->BindRoute(
		FHttpPath(TEXT("/mcp")),
		EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
		{
			auto Response = FHttpServerResponse::Create(FString(TEXT("")), TEXT("text/plain"));
			Response->Code = EHttpServerResponseCodes::Ok;
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
			Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), {TEXT("POST, GET, OPTIONS")});
			Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), {TEXT("Content-Type")});
			OnComplete(MoveTemp(Response));
			return true;
		}));

	// Must call StartAllListeners AFTER all routes are bound
	HttpModule.StartAllListeners();

	UE_LOG(LogUECortex, Log, TEXT("UECortex: HTTP server started on port %d"), Port);
}

void FMCPHttpServer::Stop()
{
	if (Router.IsValid())
	{
		FHttpServerModule::Get().StopAllListeners();
		Router.Reset();
		UE_LOG(LogUECortex, Log, TEXT("UECortex: HTTP server stopped"));
	}
}

bool FMCPHttpServer::HandlePost(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Read body — must use length-aware conversion, Request.Body is NOT null-terminated
	FUTF8ToTCHAR BodyConverter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
	FString Body(BodyConverter.Length(), BodyConverter.Get());

	// Dispatch on game thread — ALL UE API calls must be game thread
	AsyncTask(ENamedThreads::GameThread, [Body, OnComplete]()
	{
		FString ResponseBody = FMCPRouter::HandleRequest(Body);

		// Empty = notification with no response (e.g. notifications/initialized)
		if (ResponseBody.IsEmpty())
		{
			auto Response = FHttpServerResponse::Create(FString(TEXT("")), TEXT("application/json"));
			Response->Code = EHttpServerResponseCodes::NoContent;
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
			OnComplete(MoveTemp(Response));
			return;
		}

		auto Response = FHttpServerResponse::Create(ResponseBody, TEXT("application/json"));
		Response->Code = EHttpServerResponseCodes::Ok;
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
		OnComplete(MoveTemp(Response));
	});

	return true;
}

bool FMCPHttpServer::HandleSSE(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Return a minimal SSE stream — Claude Code uses this for progress notifications
	// For now: open the stream and keep it alive with a comment ping
	// Full streaming will be added in later phases
	FString InitEvent = TEXT("data: {\"type\":\"connected\",\"server\":\"UECortex\"}\n\n");

	auto Response = FHttpServerResponse::Create(InitEvent, TEXT("text/event-stream"));
	Response->Code = EHttpServerResponseCodes::Ok;
	Response->Headers.Add(TEXT("Cache-Control"), {TEXT("no-cache")});
	Response->Headers.Add(TEXT("Connection"), {TEXT("keep-alive")});
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
	OnComplete(MoveTemp(Response));

	return true;
}

bool FMCPHttpServer::HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	AsyncTask(ENamedThreads::GameThread, [OnComplete]()
	{
		auto& Registry = FMCPToolRegistry::Get();
		TArray<FString> Active, Inactive;
		Registry.GetModuleStatus(Active, Inactive);

		auto HealthObj = MakeShared<FJsonObject>();
		HealthObj->SetStringField(TEXT("status"), TEXT("ok"));
		HealthObj->SetStringField(TEXT("server"), TEXT("UECortex"));
		HealthObj->SetStringField(TEXT("version"), TEXT("1.0.0"));

		// Engine version
		FString UEVersion = FString::Printf(TEXT("%d.%d"),
			ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
		HealthObj->SetStringField(TEXT("ue_version"), UEVersion);
		HealthObj->SetNumberField(TEXT("ue_minor"), ENGINE_MINOR_VERSION);
		HealthObj->SetBoolField(TEXT("gpu_pcg_available"), UE_MCP_VERSION_5_7_PLUS ? true : false);
		HealthObj->SetNumberField(TEXT("tools_count"), Registry.GetEnabledToolCount());

		TArray<TSharedPtr<FJsonValue>> ActiveArr;
		for (const FString& M : Active)
			ActiveArr.Add(MakeShared<FJsonValueString>(M));
		HealthObj->SetArrayField(TEXT("active_modules"), ActiveArr);

		TArray<TSharedPtr<FJsonValue>> InactiveArr;
		for (const FString& M : Inactive)
			InactiveArr.Add(MakeShared<FJsonValueString>(M));
		HealthObj->SetArrayField(TEXT("inactive_modules"), InactiveArr);

		// MCP connection instructions
		HealthObj->SetStringField(TEXT("claude_code_add"),
			TEXT("claude mcp add unrealcortex --transport http http://localhost:8737/mcp"));

		FString JsonOut;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOut);
		FJsonSerializer::Serialize(HealthObj, Writer);

		auto Response = FHttpServerResponse::Create(JsonOut, TEXT("application/json"));
		Response->Code = EHttpServerResponseCodes::Ok;
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
		OnComplete(MoveTemp(Response));
	});

	return true;
}

void FMCPHttpServer::BroadcastSSE(const FString& JsonPayload)
{
	// Phase 1 stub — full SSE broadcast in later phases
	UE_LOG(LogUECortex, Log, TEXT("UECortex SSE: %s"), *JsonPayload);
}
