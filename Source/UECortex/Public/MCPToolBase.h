#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Result returned by every tool call
struct FMCPToolResult
{
	bool bSuccess = true;
	FString Message;
	TSharedPtr<FJsonObject> Data; // optional structured data

	// Image result fields
	bool bIsImage = false;
	FString ImageBase64;
	FString ImageMimeType = TEXT("image/png");

	static FMCPToolResult Success(const FString& Msg)
	{
		FMCPToolResult R;
		R.bSuccess = true;
		R.Message = Msg;
		return R;
	}

	static FMCPToolResult Success(const FString& Msg, TSharedPtr<FJsonObject> InData)
	{
		FMCPToolResult R;
		R.bSuccess = true;
		R.Message = Msg;
		R.Data = InData;
		return R;
	}

	static FMCPToolResult Error(const FString& Msg)
	{
		FMCPToolResult R;
		R.bSuccess = false;
		R.Message = Msg;
		return R;
	}

	// Returns an image content block (base64-encoded PNG by default)
	static FMCPToolResult Image(const FString& Base64Data, const FString& MimeType = TEXT("image/png"))
	{
		FMCPToolResult R;
		R.bSuccess = true;
		R.bIsImage = true;
		R.ImageBase64 = Base64Data;
		R.ImageMimeType = MimeType;
		return R;
	}
};

// Schema description for a single parameter
struct FMCPParamSchema
{
	FString Name;
	FString Type;       // "string", "number", "boolean", "object", "array"
	FString Description;
	bool bRequired;

	FMCPParamSchema(const FString& InName, const FString& InType,
	                const FString& InDesc, bool bInRequired = true)
		: Name(InName), Type(InType), Description(InDesc), bRequired(bInRequired)
	{}
};

// A single registered tool
struct FMCPToolDef
{
	FString Name;
	FString Description;
	FString Category;   // module/group label — auto-populated from module name if empty
	TArray<FMCPParamSchema> Params;
	TFunction<FMCPToolResult(const TSharedPtr<FJsonObject>&)> Handler;
	bool bEnabled = true;
};

// Base class for tool modules
class UECORTEX_API FMCPToolModuleBase
{
public:
	virtual ~FMCPToolModuleBase() = default;

	// Return the module name (e.g. "level", "blueprint")
	virtual FString GetModuleName() const = 0;

	// Register all tools into the provided array
	virtual void RegisterTools(TArray<FMCPToolDef>& OutTools) = 0;
};
