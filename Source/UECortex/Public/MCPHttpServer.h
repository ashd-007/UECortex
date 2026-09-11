#pragma once

#include "CoreMinimal.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"

class UECORTEX_API FMCPHttpServer
{
public:
	explicit FMCPHttpServer(uint32 Port = 8737);
	~FMCPHttpServer();

	void Start();
	void Stop();

	// Broadcast SSE event to all connected clients
	void BroadcastSSE(const FString& JsonPayload);

private:
	bool HandlePost(const FHttpServerRequest& Request,
	                const FHttpResultCallback& OnComplete);
	bool HandleSSE(const FHttpServerRequest& Request,
	               const FHttpResultCallback& OnComplete);
	bool HandleHealth(const FHttpServerRequest& Request,
	                  const FHttpResultCallback& OnComplete);

	uint32 Port;
	TSharedPtr<IHttpRouter> Router;

	// SSE active connections (game-thread only)
	struct FSSEConnection
	{
		FHttpResultCallback OnComplete;
		// We keep a chunked response alive by not calling OnComplete until disconnect
	};
};
