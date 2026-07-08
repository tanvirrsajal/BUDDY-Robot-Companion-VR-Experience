// BuddyAIComponent.cpp
// Full implementation: Groq AI + StreamElements TTS voice + smart follow behavior



#include "BuddyAIComponent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "AudioDecompress.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundWave.h"
#include "HAL/PlatformFilemanager.h"

UBuddyAIComponent::UBuddyAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// -------------------------------------------------------
// BEGIN PLAY — create persistent audio component
// -------------------------------------------------------
void UBuddyAIComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		VoiceAudioComponent = NewObject<UAudioComponent>(Owner);
		VoiceAudioComponent->bAutoActivate = false;
		VoiceAudioComponent->bAutoDestroy = false;
		VoiceAudioComponent->AttachToComponent(
			Owner->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform
		);
		VoiceAudioComponent->RegisterComponent();
		VoiceAudioComponent->OnAudioFinished.AddDynamic(this, &UBuddyAIComponent::HandleAudioFinished);
	}
}

// -------------------------------------------------------
// AUDIO FINISHED — fires when BUDDY's voice actually stops
// -------------------------------------------------------
void UBuddyAIComponent::HandleAudioFinished()
{
	UE_LOG(LogTemp, Warning, TEXT("BUDDY: HandleAudioFinished called - bSpeaking now false"));
	GetWorld()->GetTimerManager().ClearTimer(SpeakingTimerHandle);
	bSpeaking = false;
	OnSpeakingDone.Broadcast();
}

// -------------------------------------------------------
// TICK — runs every frame
// -------------------------------------------------------
void UBuddyAIComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Auto-find player
	if (!Player)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (PC)
		{
			Player = PC->GetPawn();
			if (Player)
			{
				// Initialize LastPlayerLocation correctly on first valid frame
				// so the very first FollowPlayer tick doesn't see a huge fake distance
				FVector InitialPos = Player->GetActorLocation();
				InitialPos.Z = GetOwner() ? GetOwner()->GetActorLocation().Z : InitialPos.Z;
				LastPlayerLocation = InitialPos;
			}
		}
	}

	if (bFollowPlayer && Player)
		FollowPlayer(DeltaTime);
}

// -------------------------------------------------------
// FOLLOW PLAYER — side offset + come-in-front logic
// -------------------------------------------------------
void UBuddyAIComponent::FollowPlayer(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Player) return;

	// Grace period: let player orientation settle before BUDDY starts evaluating targets
	if (StartupGraceTimer > 0.0f)
	{
		StartupGraceTimer -= DeltaTime;
		LastPlayerLocation = Player->GetActorLocation();
		LastPlayerLocation.Z = Owner->GetActorLocation().Z;
		return;
	}

	FVector MyPos = Owner->GetActorLocation();
	FVector PlayerPos = Player->GetActorLocation();
	PlayerPos.Z = MyPos.Z;

	// Measure player SPEED (units/sec) using frame-to-frame delta, not accumulated drift.
	// This correctly detects both "truly standing still" (speed near 0, jitter-tolerant)
	// and "actively walking" (speed clearly above jitter range) every single frame.
	float FrameDistance = FVector::Dist(PlayerPos, LastPlayerLocation);
	float PlayerSpeed = (DeltaTime > 0.0f) ? (FrameDistance / DeltaTime) : 0.0f;
	LastPlayerLocation = PlayerPos; // always update — speed-based check doesn't need a frozen anchor
	// Smooth the player's forward direction over time so a single noisy frame
	// (e.g. VR head micro-rotation) can't bias the locked front-target position
	FVector RawForward = Player->GetActorForwardVector();
	RawForward.Z = 0;
	RawForward.Normalize();
	SmoothedPlayerForward = FMath::VInterpTo(SmoothedPlayerForward, RawForward, DeltaTime, 3.0f);
	SmoothedPlayerForward.Normalize();

	float SpeedThreshold = 20.0f; // units/sec — above this means genuinely walking, below is jitter/still

	if (PlayerSpeed < SpeedThreshold)
	{
		StandStillTimer += DeltaTime;
	}
	else
	{
		StandStillTimer = 0.0f;
		bWasInFrontMode = false;
		bHasLockedSideTarget = false;
	}

	FVector TargetPos;

	// FRONT MODE DISABLED — BUDDY always stays at front-left side position
// Uncomment the if/else block below to re-enable come-to-front behavior when player stops
/*
if (StandStillTimer >= StandStillTime)
{
    if (!bWasInFrontMode)
    {
        // Small extra buffer applied ONLY at the moment of locking the front-target,
        // nowhere else — compensates for it feeling too close on arrival.
        LockedFrontTargetPos = PlayerPos + SmoothedPlayerForward * (FollowDistance - 2.0f);
        bWasInFrontMode = true;
    }
    TargetPos = LockedFrontTargetPos;
}
else
{
*/
    // Lock the side target ONCE per movement segment, ignore camera rotation
    if (!bHasLockedSideTarget)
    {
        FVector PlayerRight = Player->GetActorRightVector();
        PlayerRight.Z = 0;
        FVector PlayerForward = Player->GetActorForwardVector();
        PlayerForward.Z = 0;

        LockedSideTargetPos = PlayerPos
            + PlayerForward * (FollowDistance * 1.1f)
            - PlayerRight * (SideOffset * 0.73f);
        bHasLockedSideTarget = true;
    }
    TargetPos = LockedSideTargetPos;
/*
}
*/

	float Distance = FVector::Dist(Owner->GetActorLocation(), TargetPos); // use fresh position, not stale MyPos
	bool bWantsToMove = Distance > 50.0f;

	if (bWantsToMove && !bIsWaitingToStartWalk && WalkStartDelayTimer <= 0.0f && !bIsMoving)
	{
		bIsWaitingToStartWalk = true;
		WalkStartDelayTimer = 0.4f;
	}

	if (bIsWaitingToStartWalk)
	{
		WalkStartDelayTimer -= DeltaTime;
		if (WalkStartDelayTimer <= 0.0f)
		{
			bIsWaitingToStartWalk = false;
		}
	}

	if (!bWantsToMove)
	{
		bIsWaitingToStartWalk = false;
		WalkStartDelayTimer = 0.0f;
		bHasLockedSideTarget = false; // arrived, allow fresh target next time movement starts
	}

	if (bWantsToMove && !bIsWaitingToStartWalk)
	{
		FVector NewPos = FMath::VInterpConstantTo(MyPos, TargetPos, DeltaTime, MoveSpeed);
		FHitResult SweepHit;
		Owner->SetActorLocation(NewPos, true, &SweepHit);
		if (SweepHit.bBlockingHit)
		{
			bHasLockedSideTarget = false;
			UE_LOG(LogTemp, Warning, TEXT("BUDDY hit: %s"), 
				SweepHit.GetActor() ? *SweepHit.GetActor()->GetName() : TEXT("Unknown"));
		}
	}

	// Rotate to face player — but only at a gentle, fixed rate, never instant,
	// and never while mid-walk-start-delay (avoids any combined rotation+position snap)
	if (!bIsWaitingToStartWalk)
	{
		FVector LookDir = PlayerPos - Owner->GetActorLocation();
		LookDir.Z = 0;
		if (!LookDir.IsNearlyZero())
		{
			FRotator CurrentRot = Owner->GetActorRotation();
			FRotator TargetRot = LookDir.Rotation();
			FRotator NewRot = FMath::RInterpConstantTo(CurrentRot, TargetRot, DeltaTime, 270.0f); // max 90 deg/sec, smooth and bounded
			Owner->SetActorRotation(NewRot);
		}
	}

	bIsMoving = bWantsToMove && !bIsWaitingToStartWalk;

	// TEMP DEBUG
	// if (GEngine)
	// {
	// 	GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, FString::Printf(
	// 		TEXT("Dist=%.0f Target=%.0f,%.0f My=%.0f,%.0f Front=%d Lock=%d StandT=%.2f Speed=%.0f"),
	// 		Distance, TargetPos.X, TargetPos.Y,
	// 		Owner->GetActorLocation().X, Owner->GetActorLocation().Y,
	// 		bWasInFrontMode ? 1 : 0, bHasLockedSideTarget ? 1 : 0, StandStillTimer, PlayerSpeed
	// 	));
	// }
}
// -------------------------------------------------------
// ASK GENERAL QUESTION
// -------------------------------------------------------
void UBuddyAIComponent::AskGeneralQuestion(int32 Index)
{
    // Stop any current audio immediately
    if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
    {
        VoiceAudioComponent->Stop();
    }
    bSpeaking = false;
    bWaiting = false;

    if (!GeneralQuestions.IsValidIndex(Index)) return;
    CurrentObjectContext = TEXT("");
    SendToGroq(GeneralQuestions[Index]);
}

// -------------------------------------------------------
// ASK ABOUT A SPECIFIC LAB OBJECT
// This is called when player walks up to a robot arm,
// sensor, drone etc and presses trigger.
// ObjectName: "robot arm"
// ObjectDescription: "a 6-DOF industrial robot arm on a workbench"
// -------------------------------------------------------
void UBuddyAIComponent::AskAboutObject(const FString& ObjectName, const FString& ObjectDescription)
{
	if (bWaiting || bSpeaking) return;

	// Save the object context so follow-up questions remember it
	CurrentObjectContext = ObjectName;

	// Build a natural message that gives BUDDY context
	FString Message = FString::Printf(
		TEXT("The visitor is standing next to %s (%s). "
			 "Introduce what this is and why it matters in robotics. Keep it to 2 sentences."),
		*ObjectName, *ObjectDescription
	);

	// Clear history so BUDDY starts fresh on this object
	ChatHistory.Empty();

	// Save original prompt
	FString OriginalPrompt = SystemPrompt;

	// Override with strict object description prompt
	SystemPrompt = TEXT(
		"You are BUDDY, a robot guide in a robotics lab. "
		"Rules you must never break: "
		"1. Never start with hello, hi, hey or any greeting. "
		"2. Start by describing the color and shape of the object. "
		"3. Use the exact object name given - never invent names. "
		"4. Maximum 2 sentences only. Also use mid sized sentences not too long sentences "
		"First sentence: color, shape and name. Second sentence: what it does."
	);

	SendToGroq(Message);

	// Restore original prompt
	SystemPrompt = OriginalPrompt;
}

// -------------------------------------------------------
// ASK A FOLLOW-UP QUESTION
// Player already looked at an object, now asks more about it.
// The conversation history means BUDDY knows the context.
// Example: player looked at robot arm, then asks "how many joints does it have?"
// -------------------------------------------------------
void UBuddyAIComponent::AskFollowUp(const FString& Question)
{
	if (bWaiting || bSpeaking) return;

	FString FullQuestion = Question;

	// If we know what object we're talking about, add a reminder
	// This helps the AI stay on topic
	if (!CurrentObjectContext.IsEmpty())
	{
		FullQuestion = FString::Printf(
			TEXT("(We are talking about %s) %s"),
			*CurrentObjectContext, *Question
		);
	}

	SendToGroq(FullQuestion);
}

// -------------------------------------------------------
// CLEAR MEMORY
// Call this when player walks away from an object
// so next object starts fresh
// -------------------------------------------------------
void UBuddyAIComponent::ClearMemory()
{
	ChatHistory.Empty();
	CurrentObjectContext = TEXT("");
}

// -------------------------------------------------------
// HAPTIC FEEDBACK
// -------------------------------------------------------
void UBuddyAIComponent::Haptic(int32 Hand, float Strength)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	EControllerHand H = (Hand == 0) ? EControllerHand::Left : EControllerHand::Right;
	PC->PlayHapticEffect(nullptr, H, Strength, false);
}

// -------------------------------------------------------
// SEND TO GROQ API
// -------------------------------------------------------
void UBuddyAIComponent::SendToGroq(const FString& Message)
{
	if (GroqAPIKey == TEXT("gsk_PASTE_YOUR_GROQ_KEY_HERE"))
	{
		OnResponse.Broadcast(TEXT("Please paste your Groq API key in the Details panel!"));
		return;
	}
	// Stop any currently playing audio if new message is passed
	if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
	{
		VoiceAudioComponent->Stop();
	}
	bSpeaking = false;

	bWaiting = true;
	OnThinking.Broadcast();
	Haptic(0, 0.3f);
	bFollowPlayer = false; // BUDDY pauses while thinking

	// Add user message to history
	TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject());
	UserMsg->SetStringField("role", "user");
	UserMsg->SetStringField("content", Message);
	ChatHistory.Add(UserMsg);

	// Build messages array: system + history
	TArray<TSharedPtr<FJsonValue>> Messages;

	TSharedPtr<FJsonObject> SysMsg = MakeShareable(new FJsonObject());
	SysMsg->SetStringField("role", "system");
	SysMsg->SetStringField("content", SystemPrompt);
	Messages.Add(MakeShareable(new FJsonValueObject(SysMsg)));

	for (auto& Msg : ChatHistory)
		Messages.Add(MakeShareable(new FJsonValueObject(Msg)));

	// Build request body
	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject());
	Body->SetStringField("model", "llama-3.1-8b-instant");
	Body->SetNumberField("max_tokens", 120);
	Body->SetArrayField("messages", Messages);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL("https://api.groq.com/openai/v1/chat/completions");
	Req->SetVerb("POST");
	Req->SetHeader("Content-Type", "application/json");
	UE_LOG(LogTemp, Warning, TEXT("Using Groq key: %s"), *GroqAPIKey);
	Req->SetHeader("Authorization", "Bearer " + GroqAPIKey);
	Req->SetContentAsString(BodyStr);
	Req->OnProcessRequestComplete().BindUObject(this, &UBuddyAIComponent::OnGroqResponse);
	Req->ProcessRequest();
}

// -------------------------------------------------------
// HANDLE GROQ RESPONSE
// -------------------------------------------------------
void UBuddyAIComponent::OnGroqResponse(FHttpRequestPtr Req,
	FHttpResponsePtr Res, bool bOk)
{
	bWaiting = false;
	bFollowPlayer = true;

	if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
	{
		FString Error = !bOk ? TEXT("No internet connection.") :
			FString::Printf(TEXT("Groq error %d. Check your API key."), Res->GetResponseCode());
		OnResponse.Broadcast(Error);
		return;
	}

	// Parse JSON response
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(
		Res->GetContentAsString());
	FJsonSerializer::Deserialize(Reader, Json);

	// Safety check — if JSON failed to parse, Json will be null
	if (!Json.IsValid())
	{
		OnResponse.Broadcast(TEXT("Could not read Groq response. Try again."));
		return;
	}

	FString Answer = TEXT("I could not understand the response.");
	const TArray<TSharedPtr<FJsonValue>>* Choices;
	if (Json->TryGetArrayField("choices", Choices) && Choices->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* MsgObj;
		if ((*Choices)[0]->AsObject()->TryGetObjectField("message", MsgObj))
			(*MsgObj)->TryGetStringField("content", Answer);
	}

	// Save to history so BUDDY remembers this answer
	TSharedPtr<FJsonObject> AssistantMsg = MakeShareable(new FJsonObject());
	AssistantMsg->SetStringField("role", "assistant");
	AssistantMsg->SetStringField("content", Answer);
	ChatHistory.Add(AssistantMsg);

	// Keep history to last 12 messages (6 exchanges)
	while (ChatHistory.Num() > 12)
		ChatHistory.RemoveAt(0);

	// Fire event with text (Blueprint can show this on screen too)
	OnResponse.Broadcast(Answer);

	// Right controller buzz
	Haptic(1, 0.5f);

	// Convert to speech
	SendToTTS(Answer);
}

// -------------------------------------------------------
// SEND TEXT TO RSS
// Just change the voice= parameter to switch voices
// -------------------------------------------------------
void UBuddyAIComponent::SendToTTS(const FString& Text)
{
	bSpeaking = true;

	FString EncodedText = FGenericPlatformHttp::UrlEncode(Text);

	FString URL = FString::Printf(
		TEXT("http://api.voicerss.org/?key=48ac38xxxxx849789180cfb35888ca19&hl=en-gb&v=Nancy&r=0&c=WAV&f=44khz_16bit_mono&src=%s"),
		*EncodedText
		);

	UE_LOG(LogTemp, Warning, TEXT("Sending to StreamElements TTS: %s"), *Text);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb("GET");
	Req->SetHeader("Accept", "audio/mpeg");
	Req->OnProcessRequestComplete().BindUObject(this, &UBuddyAIComponent::OnTTSResponse);
	Req->ProcessRequest();
}

// -------------------------------------------------------
// HANDLE TTS AUDIO RESPONSE
// StreamElements sends back MP3 audio bytes
// We save to a temp file and play it using Unreal's audio system
// -------------------------------------------------------
void UBuddyAIComponent::OnTTSResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
    {
        bSpeaking = false;
        OnSpeakingDone.Broadcast();
        return;
    }

    const TArray<uint8>& AudioData = Res->GetContent();

    // DO NOT strip anything, DO NOT assume PCM
    // NOTE: OnSpeakingDone is now broadcast by HandleAudioFinished()
    // when the audio actually finishes playing — not here.
    PlayAudioData(AudioData);
}

// -------------------------------------------------------
// PLAY AUDIO DATA
// Creates a procedural sound wave from raw PCM bytes
// and plays it at BUDDY's location in 3D space
// -------------------------------------------------------
void UBuddyAIComponent::PlayAudioData(const TArray<uint8>& AudioData)
{
    AActor* Owner = GetOwner();
    if (!Owner || AudioData.Num() == 0 || !VoiceAudioComponent) return;

    // Stop whatever's currently playing first
    if (VoiceAudioComponent->IsPlaying())
    {
        VoiceAudioComponent->Stop();
    }

    // Save WAV file
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("BuddyTTS.wav");
    FFileHelper::SaveArrayToFile(AudioData, *FilePath);

    // Create SoundWave procedural (runtime-safe)
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>();

    if (!SoundWave) return;

    SoundWave->SetSampleRate(44100);
    SoundWave->NumChannels = 1;
    SoundWave->SoundGroup = SOUNDGROUP_Voice;
    SoundWave->bLooping = false;

    // Load file as PCM buffer (IMPORTANT)
    TArray<uint8> RawFile;
    if (!FFileHelper::LoadFileToArray(RawFile, *FilePath))
        return;

    FWaveModInfo WaveInfo;
    if (!WaveInfo.ReadWaveInfo(RawFile.GetData(), RawFile.Num()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid WAV format"));
        return;
    }

    // Feed ONLY PCM data (not headers)
    SoundWave->QueueAudio(
        WaveInfo.SampleDataStart,
        WaveInfo.SampleDataSize
    );

    VoiceAudioComponent->SetSound(SoundWave);
    VoiceAudioComponent->Play();
    bSpeaking = true;

    // Calculate actual audio duration in seconds: bytes / (sample rate * channels * bytes-per-sample)
    float DurationSeconds = (float)WaveInfo.SampleDataSize / (44100.0f * 1 * 2.0f);

    // Backup timer in case OnAudioFinished doesn't fire reliably for procedural audio
    GetWorld()->GetTimerManager().ClearTimer(SpeakingTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(
        SpeakingTimerHandle,
        this,
        &UBuddyAIComponent::HandleAudioFinished,
        DurationSeconds + 0.2f, // small buffer
        false
    );
}

void UBuddyAIComponent::StartRecording()
{
    if (bIsRecording || bWaiting) return;

    // Barge-in: if BUDDY is currently talking, interrupt it
    if (bSpeaking)
    {
        StopSpeaking();
    }

    bIsRecording = true;
    RecordingBuffer.Empty();

    Audio::FAudioCaptureDeviceParams Params;

    AudioCapture.OpenCaptureStream(
        Params,
        [this]
        (const float* AudioData,
         int32 NumFrames,
         int32 NumChannels,
         int32 SampleRate,
         double StreamTime,
         bool bOverFlow)
        {
            if (!bIsRecording) return;

            // store real device format
            CapturedSampleRate = SampleRate;
            CapturedNumChannels = NumChannels;

            int32 TotalSamples = NumFrames * NumChannels;

            for (int32 i = 0; i < TotalSamples; i++)
            {
                // 🔥 FIX 1: audio gain boost (THIS fixes low volume)
                float SampleF = AudioData[i] * 3.0f;

                // clamp to avoid distortion explosion
                SampleF = FMath::Clamp(SampleF, -1.0f, 1.0f);

                int16 Sample = (int16)(SampleF * 32767.0f);

                RecordingBuffer.Add((uint8)(Sample & 0xFF));
                RecordingBuffer.Add((uint8)((Sample >> 8) & 0xFF));
            }
        },
        4096 // 🔥 FIX 2: bigger buffer = no lag / no chop
    );

    AudioCapture.StartStream();

    UE_LOG(LogTemp, Warning, TEXT("BUDDY: Recording started... SR=%d CH=%d"),
        CapturedSampleRate, CapturedNumChannels);
}

void UBuddyAIComponent::StopRecordingAndTranscribe()
{
    if (!bIsRecording) return;
    bIsRecording = false;

    AudioCapture.StopStream();
    AudioCapture.CloseStream();

    UE_LOG(LogTemp, Warning, TEXT("BUDDY: Recording stopped. Buffer size: %d"),
        RecordingBuffer.Num());

    if (RecordingBuffer.Num() > 0)
    {
        TArray<uint8> WavData;

        // 🔥 FIX: use REAL captured values
        int32 SampleRate = CapturedSampleRate;
        int32 NumChannels = CapturedNumChannels;

        int32 BitsPerSample = 16;
        int32 DataSize = RecordingBuffer.Num();
        int32 ByteRate = SampleRate * NumChannels * BitsPerSample / 8;
        int32 BlockAlign = NumChannels * BitsPerSample / 8;

        auto WriteInt32 = [&](int32 Value)
        {
            WavData.Add(Value & 0xFF);
            WavData.Add((Value >> 8) & 0xFF);
            WavData.Add((Value >> 16) & 0xFF);
            WavData.Add((Value >> 24) & 0xFF);
        };

        auto WriteInt16 = [&](int16 Value)
        {
            WavData.Add(Value & 0xFF);
            WavData.Add((Value >> 8) & 0xFF);
        };

        auto WriteStr = [&](const char* Str)
        {
            while (*Str)
                WavData.Add(*Str++);
        };

        WriteStr("RIFF");
        WriteInt32(36 + DataSize);
        WriteStr("WAVE");
        WriteStr("fmt ");
        WriteInt32(16);
        WriteInt16(1);
        WriteInt16(NumChannels);
        WriteInt32(SampleRate);
        WriteInt32(ByteRate);
        WriteInt16(BlockAlign);
        WriteInt16(BitsPerSample);
        WriteStr("data");
        WriteInt32(DataSize);

        WavData.Append(RecordingBuffer);

        FString DebugPath = FPaths::ProjectSavedDir() + TEXT("BuddyRecording.wav");
        FFileHelper::SaveArrayToFile(WavData, *DebugPath);

        UE_LOG(LogTemp, Warning, TEXT("Saved recording to: %s"), *DebugPath);

        SendToGroqWhisper(WavData);
    }
}

void UBuddyAIComponent::StopSpeaking()
{
    GetWorld()->GetTimerManager().ClearTimer(SpeakingTimerHandle);
    if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
    {
        VoiceAudioComponent->Stop();
    }
    bSpeaking = false;
    OnSpeakingDone.Broadcast();
}

void UBuddyAIComponent::SendToGroqWhisper(const TArray<uint8>& AudioData)
{
	FString URL = TEXT("https://api.groq.com/openai/v1/audio/transcriptions");

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb("POST");
	UE_LOG(LogTemp, Warning, TEXT("Using Groq key: %s"), *GroqAPIKey);
	Req->SetHeader("Authorization", "Bearer " + GroqAPIKey);

	FString Boundary = TEXT("----BuddyBoundary");
	Req->SetHeader("Content-Type", FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));

	TArray<uint8> Body;
	FString Part1 = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-large-v3\r\n--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nen\r\n"), *Boundary, *Boundary);
	FString Part2 = FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n"), *Boundary);
	FString Part3 = FString::Printf(TEXT("\r\n--%s--\r\n"), *Boundary);

	Body.Append((uint8*)TCHAR_TO_UTF8(*Part1), Part1.Len());
	Body.Append((uint8*)TCHAR_TO_UTF8(*Part2), Part2.Len());
	Body.Append(AudioData);
	Body.Append((uint8*)TCHAR_TO_UTF8(*Part3), Part3.Len());

	Req->SetContent(Body);
	Req->OnProcessRequestComplete().BindUObject(this, &UBuddyAIComponent::OnWhisperResponse);
	Req->ProcessRequest();
}

void UBuddyAIComponent::OnWhisperResponse(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("BUDDY: Whisper error %d"), Res.IsValid() ? Res->GetResponseCode() : 0);
		return;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
	FJsonSerializer::Deserialize(Reader, Json);

	if (!Json.IsValid()) return;

	FString TranscribedText;
	if (Json->TryGetStringField("text", TranscribedText) && !TranscribedText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("BUDDY heard: %s"), *TranscribedText);
		OnHeard.Broadcast(TranscribedText);
		SendToGroq(TranscribedText);
	}
}