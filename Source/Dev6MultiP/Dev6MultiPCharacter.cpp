// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev6MultiPCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "ThirdPersonProjectile.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"

	
//////////////////////////////////////////////////////////////////////////
// ADev6MultiPCharacter

ADev6MultiPCharacter::ADev6MultiPCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	maxHealth = 100.0f;
	currentHealth = maxHealth;

	ProjectileClass = AThirdPersonProjectile::StaticClass();
	FireRate = 0.25f;
	bIsFiringWeapon = false;
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ADev6MultiPCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ADev6MultiPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADev6MultiPCharacter, currentHealth);
}

void ADev6MultiPCharacter::StartFire()
{
	if (!bIsFiringWeapon)
	{
		bIsFiringWeapon = true;
		UWorld* World = GetWorld();
		World->GetTimerManager().SetTimer(FiringTimer, this, &ADev6MultiPCharacter::StopFire, FireRate, false);
		HandleFire();
	}
}

void ADev6MultiPCharacter::StopFire()
{
	bIsFiringWeapon = false;
}

void ADev6MultiPCharacter::HandleFire_Implementation()
{
	FVector spawnLocation = GetActorLocation() + ( GetActorRotation().Vector()  * 100.0f ) + (GetActorUpVector() * 50.0f);
	FRotator spawnRotation = GetActorRotation();

	FActorSpawnParameters spawnParameters;
	spawnParameters.Instigator = GetInstigator();
	spawnParameters.Owner = this;

	AThirdPersonProjectile* spawnedProjectile = GetWorld()->SpawnActor<AThirdPersonProjectile>(spawnLocation, spawnRotation, spawnParameters);
}

void ADev6MultiPCharacter::OnRep_CurrentHealth()
{
	OnHealthUpdate();
}

void ADev6MultiPCharacter::OnHealthUpdate()
{
	if(IsLocallyControlled())
	{
		FString healtMessage = FString::Printf(TEXT("You now have %f health remaining."), currentHealth);
		GEngine->AddOnScreenDebugMessage(-1,5.f, FColor::Blue, healtMessage);
		
		if ((currentHealth <= 0))
		{
			FString deadMessage = FString::Printf(TEXT("You have been killed."));
			GEngine->AddOnScreenDebugMessage(-1,5.f,FColor::Red,deadMessage);
		}
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		FString healthMessage = FString::Printf(TEXT("%s now has %f health remaining."), *GetFName().ToString(), currentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, healthMessage);
	}
}

//press 1
void ADev6MultiPCharacter::OpenLobby()
{
	UWorld* World = GetWorld();

	if(World)
	{
		World->ServerTravel("/Game/maps/Map_ListenServer.Map_ListenServer?listen");
	}
}

//press 2
void ADev6MultiPCharacter::CallOpenLevel(const FString& Address)
{
	UGameplayStatics::OpenLevel(this, *Address);
}

//press 3
void ADev6MultiPCharacter::CallClientTravel(const FString& Address)
{
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	
	if(PlayerController)
	{
		PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void ADev6MultiPCharacter::SetCurrentHealth(float healthValue)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		currentHealth = FMath::Clamp(healthValue, 0.f, maxHealth);
		OnHealthUpdate();
	}
}

float ADev6MultiPCharacter::TakeDamage(float DamageTaken, FDamageEvent const& DamageEvent, AController* EventInstigator,
	AActor* DamageCauser)
{
	float damageApplied = currentHealth - DamageTaken;
	SetCurrentHealth(damageApplied);
	return damageApplied;
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADev6MultiPCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ADev6MultiPCharacter::StartFire);
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADev6MultiPCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADev6MultiPCharacter::Look);

	}
}


void ADev6MultiPCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ADev6MultiPCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}






