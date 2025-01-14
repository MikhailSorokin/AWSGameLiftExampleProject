#include "MatchmakingWidget.h"
#include "GameLiftTutorial.h"
#include "GameLiftTutorialGameInstance.h"
#include "TextReaderComponent.h"
#include "TimerManager.h"
#include "Components/TextBlock.h"
//#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Json.h"
#include "JsonUtilities.h"

void UMatchmakingWidget::NativeConstruct()
{
	Super::NativeConstruct();


	winsTextBlock = (UTextBlock*)GetWidgetFromName(TEXT("Wins"));
	LossesTextBlock = (UTextBlock*)GetWidgetFromName(TEXT("Losses"));
	MatchmakingStatusTextBlock = (UTextBlock*)GetWidgetFromName(TEXT("MatchmakingStatus"));
	
	GetWorld()->GetTimerManager().SetTimer(SetAveragePlayerLatencyHandle, this, &UMatchmakingWidget::SetAveragePlayerLatency, 1.0f, true, 1.0f);

}

void UMatchmakingWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(PollMatchmakingHandle);
	GetWorld()->GetTimerManager().ClearTimer(SetAveragePlayerLatencyHandle);
	Super::NativeDestruct();
}


UMatchmakingWidget::UMatchmakingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	UTextReaderComponent* TextReader = CreateDefaultSubobject<UTextReaderComponent>(TEXT("TextReaderComp"));

	FString ApiUrl = TextReader->ReadFile("Urls/ApiUrl.txt");

	StartMatchLookupUrl = ApiUrl + "/startmatchmaking";
	CancelMatchLookupUrl = ApiUrl + "/stopmatchmaking";
	PollMatchmakingUrl = ApiUrl + "/pollmatchmaking";
	GetPlayerDataUrl = ApiUrl + "/getplayerdata";
	RegionCode = TextReader->ReadFile("Urls/RegionCode.txt");
	HttpModule = &FHttpModule::Get();
	SearchingForGame = false;
}

void UMatchmakingWidget::BeginMatchmaking(int32 InNumPlayers)
{
	//TODO - Get ID from google cognito and log into AWS service with player ID
	//disable the button after clicked as to not cause two responses to be sent
	UGameLiftTutorialGameInstance* GLGI = Cast<UGameLiftTutorialGameInstance>(GetGameInstance());
	MatchmakingTicketId = GLGI->MatchmakingTicketId;
	AccessToken = GLGI->AccessToken;

	if (!SearchingForGame)
	{

		//Reset variables here related to searching
		SearchingForGame = true;

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Got into Begin matchmaking");
		if (AccessToken.Len() > 0) {
			TSharedRef<FJsonObject> LatencyMapObj = MakeShareable(new FJsonObject);
			LatencyMapObj->SetNumberField(RegionCode, AveragePlayerLatency); //If more then one region, each region would have its own value

			TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
			RequestObj->SetObjectField("latencyMap", LatencyMapObj);

			FString RequestBody;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);

			if (FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer)) {
				// send a get request to google discovery document to retrieve endpoints
				TSharedRef<IHttpRequest> StopMatchmakingRequest = HttpModule->CreateRequest();
				StopMatchmakingRequest->OnProcessRequestComplete().BindUObject(this, &UMatchmakingWidget::OnStartMatchmakingResponseReceived);
				StopMatchmakingRequest->SetURL(StartMatchLookupUrl);
				StopMatchmakingRequest->SetVerb("POST");
				StopMatchmakingRequest->SetHeader("Content-Type", "application/json");
				StopMatchmakingRequest->SetHeader("Authorization", AccessToken);
				StopMatchmakingRequest->SetContentAsString(RequestBody);
				StopMatchmakingRequest->ProcessRequest();
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("at line 77"));
			}
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("at line 80"));
		}
	}



}


void UMatchmakingWidget::OnStartMatchmakingResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
	//UE_LOG(LogTemp, Warning, TEXT("Response from initiate matchmaking %s"), *(Response->GetContentAsString()));

	if (bWasSuccessful) {
		//Create a pointer to hold the json serialized data
		TSharedPtr<FJsonObject> JsonObject;

		//Create a reader pointer to read the json data
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			UE_LOG(LogAWS, Log, TEXT("%s   response: %s"), __FUNCTIONW__, *Response->GetContentAsString());

			MatchmakingTicketId = JsonObject->GetStringField("ticketId");

			UGameInstance* GameInstance = GetGameInstance();
			if (GameInstance != nullptr) {
				UGameLiftTutorialGameInstance* GameLiftTutorialGameInstance = Cast<UGameLiftTutorialGameInstance>(GameInstance);
				UE_LOG(LogAWS, Log, TEXT("Assigning matchmaking ticket %s"), *(MatchmakingTicketId));
				GameLiftTutorialGameInstance->MatchmakingTicketId = MatchmakingTicketId;

				GetWorld()->GetTimerManager().SetTimer(PollMatchmakingHandle, this, &UMatchmakingWidget::PollMatchmaking, 1.0f, true, 1.0f);
				SearchingForGame = true;
			}

			//AWS recommends to only poll every 10 seconds for optimization
			//GetWorld()->GetTimerManager().SetTimer(PollMatchmakingHandle, this, &UMatchmakingWidget::PollMatchmaking, 1.0f, false, 10.0f);
			//SearchingForGame = true;


			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Initializing matchmaking request");

			

			//TODO - Handle currently looking for a match with widget stuff
		}
	}
}


void UMatchmakingWidget::PollMatchmaking()
{

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr)
	{
		UGameLiftTutorialGameInstance* GLGI = Cast<UGameLiftTutorialGameInstance>(GameInstance);
		AccessToken = GLGI->AccessToken;
		MatchmakingTicketId = GLGI->MatchmakingTicketId;

	}

	if (AccessToken.Len() > 0 && MatchmakingTicketId.Len() > 0 && SearchingForGame)
	{
		UE_LOG(LogAWS, Log, TEXT("%s: in poll matchmaking"), __FUNCTIONW__);

		TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
		RequestObj->SetStringField("ticketId", MatchmakingTicketId);

		FString RequestBody;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
		if (FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer))
		{
			TSharedRef<IHttpRequest> PollMatchmakingRequest = HttpModule->CreateRequest();
			PollMatchmakingRequest->OnProcessRequestComplete().BindUObject(this, &UMatchmakingWidget::OnPollMatchmakingResponseReceived);
			PollMatchmakingRequest->SetURL(PollMatchmakingUrl);
			PollMatchmakingRequest->SetVerb("POST");
			PollMatchmakingRequest->SetHeader("Content-type", "application/json");
			PollMatchmakingRequest->SetHeader("Authorization", AccessToken);
			PollMatchmakingRequest->SetContentAsString(RequestBody);
			PollMatchmakingRequest->ProcessRequest();

		}
	}
}

void UMatchmakingWidget::OnPollMatchmakingResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogAWS, Log, TEXT("%s: Response from initiate matchmaking %s"), __FUNCTIONW__, *(Response->GetContentAsString()));

	if (bWasSuccessful && SearchingForGame) {
		//Create a pointer to hold the json serialized data
		TSharedPtr<FJsonObject> JsonObject;
		//Create a reader pointer to read the json data
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

		//Deserialize the json data given Reader and the actual object to deserialize
		if (FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			if (JsonObject->HasField("ticket")) {
				TSharedPtr<FJsonObject> Ticket = JsonObject->GetObjectField("ticket");
				FString TicketType = Ticket->GetObjectField("Type")->GetStringField("S");

				if (TicketType.Len() > 0)
				{
					// continue to poll matchmaking
					GetWorld()->GetTimerManager().ClearTimer(PollMatchmakingHandle);
					SearchingForGame = false;

					UGameInstance* GameInstance = GetGameInstance();
					if (GameInstance != nullptr) {
						UGameLiftTutorialGameInstance* GameLiftTutorialGameInstance = Cast<UGameLiftTutorialGameInstance>(GameInstance);
						if (GameLiftTutorialGameInstance != nullptr) {
							GameLiftTutorialGameInstance->MatchmakingTicketId = FString("");
						}
					}


					if (TicketType.Compare("MatchmakingSucceeded") == 0) {
						// if check is to deal with a race condition involving the user pressing the cancel button
						//TODO - Reset the widget and say we successfully found a match
						MatchmakingStatusTextBlock->SetText(FText::FromString("Successfully found a match. Now connecting to the server"));

						// get the game session and player session details and connect to the server
						TSharedPtr<FJsonObject> GameSessionInfo = Ticket->GetObjectField("GameSessionInfo")->GetObjectField("M");
						FString IpAddress = GameSessionInfo->GetObjectField("IpAddress")->GetStringField("S");
						FString Port = GameSessionInfo->GetObjectField("Port")->GetStringField("N");

						TArray<TSharedPtr<FJsonValue>> Players = Ticket->GetObjectField("Players")->GetArrayField("L");
						TSharedPtr<FJsonObject> Player = Players[0]->AsObject()->GetObjectField("M");
						FString PlayerSessionId = Player->GetObjectField("PlayerSessionId")->GetStringField("S");
						FString PlayerId = Player->GetObjectField("PlayerId")->GetStringField("S");

						FString LevelName = IpAddress + FString(":") + Port;
						const FString& Options = FString("?") + FString("PlayerSessionId=") + PlayerSessionId + FString("?PlayerId=") + PlayerId;
						UE_LOG(LogAWS, Warning, TEXT("Options: %s"), *Options);

						//connect to the server here!!!!! might need to check to see if this fails. OnTravel function
						UGameplayStatics::OpenLevel(GetWorld(), FName(*LevelName), false, Options);

					}
					else
					{
						MatchmakingStatusTextBlock->SetText(FText::FromString("Something went wrong, please try again"));
					}
				}
			}
		}
	}
}


void UMatchmakingWidget::CancelMatchmaking()
{
	if (SearchingForGame) { //when starting a match via button need to set this variable to true
		GetWorld()->GetTimerManager().ClearTimer(PollMatchmakingHandle); // stop searching for a match
		SearchingForGame = false;
		UE_LOG(LogTemp, Warning, TEXT("Cancel matchmaking"));

		if (MatchmakingTicketId.Len() > 0 && AccessToken.Len() > 0) {
			// cancel matchmaking request
			TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
			RequestObj->SetStringField("ticketId", MatchmakingTicketId);

			FString RequestBody;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);

			if (FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer)) {
				// send a get request to google discovery document to retrieve endpoints
				TSharedRef<IHttpRequest> StopMatchmakingRequest = HttpModule->CreateRequest();
				StopMatchmakingRequest->OnProcessRequestComplete().BindUObject(this, &UMatchmakingWidget::OnStopMatchmakingResponseReceived);
				StopMatchmakingRequest->SetURL(CancelMatchLookupUrl);
				StopMatchmakingRequest->SetVerb("POST");
				StopMatchmakingRequest->SetHeader("Content-Type", "application/json");
				StopMatchmakingRequest->SetHeader("Authorization", AccessToken);
				StopMatchmakingRequest->SetContentAsString(RequestBody);
				StopMatchmakingRequest->ProcessRequest();
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("at line 247"));
				//need to make sure the buttons are enabled and that 
				/*UTextBlock* ButtonText = (UTextBlock*)MatchmakingButton->GetChildAt(0);
				ButtonText->SetText(FText::FromString("Join Game"));
				MatchmakingEventTextBlock->SetText(FText::FromString(""));
				MatchmakingButton->SetIsEnabled(true);*/
			}
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("at line 255"));
		}
	}
}

void UMatchmakingWidget::OnStopMatchmakingResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
	//UE_LOG(LogTemp, Warning, TEXT("Response from cancel matchmaking %s"), *(Response->GetContentAsString()));

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr) {
		UGameLiftTutorialGameInstance* GameLiftTutorialGameInstance = Cast<UGameLiftTutorialGameInstance>(GameInstance);
		if (GameLiftTutorialGameInstance != nullptr) {
			GameLiftTutorialGameInstance->MatchmakingTicketId = FString("");
		}
		UE_LOG(LogAWS, Log, TEXT("%s: cancellation successful"), __FUNCTIONW__);
	}

}


void UMatchmakingWidget::FetchCurrentTokenStatus()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr) {
		UGameLiftTutorialGameInstance* GameLiftTutorialGameInstance = Cast<UGameLiftTutorialGameInstance>(GameInstance);
		if (GameLiftTutorialGameInstance != nullptr) {
			MatchmakingTicketId = GameLiftTutorialGameInstance->MatchmakingTicketId;
			AccessToken = GameLiftTutorialGameInstance->AccessToken;
		}
	}
}

//get player latency from get instance
void UMatchmakingWidget::SetAveragePlayerLatency()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr)
	{
		UGameLiftTutorialGameInstance* GameLiftTutorialGameInstance = Cast<UGameLiftTutorialGameInstance>(GameInstance);
		if (GameLiftTutorialGameInstance != nullptr)
		{
			float TotalPlayerLatency = 0.0f;
			for (float i : GameLiftTutorialGameInstance->PlayerLatencies)
			{
				TotalPlayerLatency += i;
			}

			if (TotalPlayerLatency > 0)
			{
				AveragePlayerLatency = TotalPlayerLatency / GameLiftTutorialGameInstance->PlayerLatencies.Num();

				//now add averagePlayerLatency to a text box or whatever else. We can round this value to int
				FString PingString = "Ping:" + FString::FromInt(FMath::RoundToInt(AveragePlayerLatency)) + "ms";
			}
		}
	}
}



void UMatchmakingWidget::getPlayerData()
{

	//Get player win loss data if we would like to add this functionality to our widgets
	TSharedRef<IHttpRequest> GetPlayerDataRequest = HttpModule->CreateRequest();
	GetPlayerDataRequest->OnProcessRequestComplete().BindUObject(this, &UMatchmakingWidget::OnGetPlayerDataResponseReceived);
	GetPlayerDataRequest->SetURL(GetPlayerDataUrl);
	GetPlayerDataRequest->SetVerb("GET");
	GetPlayerDataRequest->SetHeader("Authorization", AccessToken);
	GetPlayerDataRequest->SetHeader("Content-Type", "application/json");
	GetPlayerDataRequest->ProcessRequest();
}


void UMatchmakingWidget::OnGetPlayerDataResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, JsonObject))
		{
			if (JsonObject->HasField("playerData"))
			{
				TSharedPtr<FJsonObject> PlayerData = JsonObject->GetObjectField("playerData");
				TSharedPtr<FJsonObject> WinsObject = PlayerData->GetObjectField("Wins");
				TSharedPtr<FJsonObject> LossesObject = PlayerData->GetObjectField("Losses");

				FString Wins = WinsObject->GetStringField("N");
				FString Losses = LossesObject->GetStringField("N");


				
				winsTextBlock->SetText(FText::FromString("Wins: " + Wins));
				LossesTextBlock->SetText(FText::FromString("Losses: " + Losses));

			}
		}
	}
}
