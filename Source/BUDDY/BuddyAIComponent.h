#pragma once

#include "Misc/FileHelper.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Http.h"
#include "AudioCapture.h"
#include "AudioCaptureCore.h"
#include "Sound/SoundWaveProcedural.h"
#include "BuddyAIComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuddyResponse, const FString&, ResponseText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBuddyThinking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBuddySpeaking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuddyHeard, const FString&, HeardText);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BUDDY_API UBuddyAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuddyAIComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

    virtual void BeginPlay() override;

	// -------------------------------------------------------
	// API KEYS — paste both here
	// -------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Keys")
	FString GroqAPIKey = TEXT("gsk_CZ0dgtppQ4YH77s6GLmhWQe4z0uJn");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Keys")
	FString ElevenLabsAPIKey = TEXT("sk_cbb94a6c1f18f058204cb13473");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Keys")
	FString ElevenLabsVoiceID = TEXT("r1KmysJdVYZjJCm4mL3b");
	
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Keys")
	// FString TTSVoice = TEXT("bella");	
	// -------------------------------------------------------
	// BUDDY'S PERSONALITY
	// -------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Personality", meta=(MultiLine=true))
		FString SystemPrompt = TEXT(
		"You are BUDDY, a friendly robot companion in a robotics research lab. "
		"Answer ANY question the visitor asks — not just about robotics. "
		"Be helpful, cheerful and enthusiastic. "
		"Keep answers to 2-3 sentences maximum. "
		"Speak naturally and conversationally. "
		"Never redirect the conversation or suggest alternatives — just answer directly."
	);

	// General questions shown on BUDDY's screen
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Questions")
	TArray<FString> GeneralQuestions = {
		TEXT("What is this lab used for?"),
		TEXT("What are you made of?"),
		TEXT("How do robots navigate?"),
		TEXT("What is artificial intelligence?"),
		TEXT("What is the most advanced robot today?"),
		TEXT("How do robots learn new things?")
	};

	// -------------------------------------------------------
	// FOLLOW BEHAVIOR
	// -------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Follow")
	float FollowDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Follow")
	float SideOffset = 135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Follow")
	float MoveSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Follow")
	float TurnSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BUDDY|Follow")
	float StandStillTime = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category="BUDDY|Follow")
	bool bFollowPlayer = true;

	UPROPERTY(BlueprintReadWrite, Category="BUDDY|Follow")
	AActor* Player = nullptr;

	// -------------------------------------------------------
	// VOICE RECORDING — NEW
	// -------------------------------------------------------
	UPROPERTY(BlueprintReadOnly, Category="BUDDY|Voice")
	bool bIsRecording = false;

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void StopRecordingAndTranscribe();

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void StopSpeaking();

	// -------------------------------------------------------
	// CALLABLE FROM BLUEPRINT
	// -------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void AskGeneralQuestion(int32 Index);

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void AskAboutObject(const FString& ObjectName, const FString& ObjectDescription);

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void AskFollowUp(const FString& Question);

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void Haptic(int32 Hand, float Strength = 0.5f);

	UFUNCTION(BlueprintCallable, Category="BUDDY")
	void ClearMemory();

	UPROPERTY(BlueprintReadOnly, Category="BUDDY")
	bool bWaiting = false;

	UPROPERTY(BlueprintReadOnly, Category="BUDDY")
	bool bSpeaking = false;

	UPROPERTY(BlueprintReadOnly, Category="BUDDY")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category="BUDDY")
	FString CurrentObjectContext = TEXT("");

	// Events Blueprint binds to
	UPROPERTY(BlueprintAssignable, Category="BUDDY")
	FOnBuddyResponse OnResponse;

	UPROPERTY(BlueprintAssignable, Category="BUDDY")
	FOnBuddyThinking OnThinking;

	UPROPERTY(BlueprintAssignable, Category="BUDDY")
	FOnBuddySpeaking OnSpeakingDone;

	UPROPERTY(BlueprintAssignable, Category="BUDDY")
	FOnBuddyHeard OnHeard;

	UPROPERTY(EditAnywhere)
	float MeshYawOffset = 90.0f;

private:
	// AI
	void SendToGroq(const FString& Message);
	void OnGroqResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);

	int32 CapturedSampleRate = 48000;
	int32 CapturedNumChannels = 1;

	// Voice output
	// Voice output
	void SendToTTS(const FString& Text);
	void OnTTSResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void PlayAudioData(const TArray<uint8>& AudioData);

	UFUNCTION()
	void HandleAudioFinished();

	// Voice input — NEW
	void SendToGroqWhisper(const TArray<uint8>& AudioData);
	void OnWhisperResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);

	// Follow
	void FollowPlayer(float DeltaTime);

	// Memory
	TArray<TSharedPtr<FJsonObject>> ChatHistory;

	// Stand-still timer
	float StandStillTimer = 0.0f;
	FVector LastPlayerLocation = FVector::ZeroVector;
	FVector SmoothedPlayerForward = FVector::ForwardVector;
	bool bWasInFrontMode = false;
	FVector LockedFrontTargetPos = FVector::ZeroVector;
	bool bHasLockedSideTarget = false;
	FVector LockedSideTargetPos = FVector::ZeroVector;
	float StartupGraceTimer = 1.0f; // ignore movement logic for first second after BeginPlay

	// Walk start delay
	float WalkStartDelayTimer = 0.0f;
	bool bIsWaitingToStartWalk = false;

	// Audio component for playing BUDDY's voice
	UPROPERTY()
	UAudioComponent* VoiceAudioComponent = nullptr;
	
	// Timer for reliably detecting when TTS audio finishes
	FTimerHandle SpeakingTimerHandle;

	// Recording buffer
	TArray<uint8> RecordingBuffer;

	// Microphone capture
    Audio::FAudioCapture AudioCapture;
};