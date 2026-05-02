// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGP_2526Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "InputActionValue.h"
#include "MGP_2526.h"
#include "TeleportAnchorProjectile.h"

AMGP_2526Character::AMGP_2526Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// First person needs the player to turn with the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Reusing the boom, just pulled into the head for first person.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 88.0f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Hides the template body from our own camera.
	GetMesh()->SetOwnerNoSee(true);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	AnchorProjectileClass = ATeleportAnchorProjectile::StaticClass();
}

void AMGP_2526Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);
	}
	else
	{
		UE_LOG(LogMGP_2526, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMGP_2526Character::ThrowStraightAnchor);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMGP_2526Character::ThrowLobAnchor);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMGP_2526Character::TeleportToAnchor);
}

void AMGP_2526Character::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMGP_2526Character::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMGP_2526Character::ThrowStraightAnchor()
{
	// Flatter throw.
	ThrowAnchor(StraightThrowSpeed, 0.03f, 0.25f);
}

void AMGP_2526Character::ThrowLobAnchor()
{
	// Higher throw.
	ThrowAnchor(LobThrowSpeed, 0.35f, 0.9f);
}

void AMGP_2526Character::ThrowAnchor(float Speed, float UpwardsAim, float GravityScale)
{
	if (!AnchorProjectileClass || !FollowCamera)
	{
		return;
	}

	if (ActiveAnchor)
	{
		// Only keeping one anchor active for this first version.
		ActiveAnchor->Destroy();
		ActiveAnchor = nullptr;
	}

	const FVector CameraDirection = FollowCamera->GetForwardVector();
	const FVector ThrowDirection = (CameraDirection + FVector::UpVector * UpwardsAim).GetSafeNormal();

	// Spawning it slightly to the right feels closer to a hand throw.
	const FVector HandOffset = FollowCamera->GetRightVector() * 28.0f - FVector::UpVector * 18.0f;
	const FVector SpawnLocation = FollowCamera->GetComponentLocation() + CameraDirection * AnchorSpawnDistance + HandOffset;
	const FRotator SpawnRotation = ThrowDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	// The spawn should not fail just because the player is near a wall.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveAnchor = GetWorld()->SpawnActor<ATeleportAnchorProjectile>(
		AnchorProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (ActiveAnchor)
	{
		ActiveAnchor->LaunchAnchor(ThrowDirection, Speed, GravityScale);
	}
}

void AMGP_2526Character::TeleportToAnchor()
{
	if (!ActiveAnchor)
	{
		ShowAnchorMessage(TEXT("No anchor"));
		return;
	}

	if (!ActiveAnchor->HasLanded())
	{
		ShowAnchorMessage(TEXT("Anchor not ready"));
		return;
	}

	FVector TeleportLocation;
	if (!FindSafeTeleportLocation(TeleportLocation))
	{
		return;
	}

	TeleportTo(TeleportLocation, GetActorRotation(), false, true);

	ActiveAnchor->Destroy();
	ActiveAnchor = nullptr;
}

bool AMGP_2526Character::FindSafeTeleportLocation(FVector& OutLocation) const
{
	if (!ActiveAnchor || !GetWorld())
	{
		return false;
	}

	const float DistanceToAnchor = FVector::Dist(GetActorLocation(), ActiveAnchor->GetActorLocation());
	if (DistanceToAnchor > MaxTeleportDistance)
	{
		ShowAnchorMessage(TEXT("Anchor too far"));
		return false;
	}

	const FVector SurfaceNormal = ActiveAnchor->GetSurfaceNormal();
	if (SurfaceNormal.Z < -0.2f)
	{
		ShowAnchorMessage(TEXT("Teleport blocked"));
		return false;
	}

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const FVector SurfacePoint = ActiveAnchor->GetSurfacePoint();
	FVector CheckStart = SurfacePoint + SurfaceNormal * (CapsuleRadius + 35.0f) + FVector::UpVector * 120.0f;
	FVector CheckEnd = CheckStart - FVector::UpVector * 350.0f;

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AnchorTeleportGround), false, this);
	QueryParams.AddIgnoredActor(ActiveAnchor);

	if (!GetWorld()->LineTraceSingleByChannel(GroundHit, CheckStart, CheckEnd, ECC_Visibility, QueryParams))
	{
		ShowAnchorMessage(TEXT("No ground"));
		return false;
	}

	if (GroundHit.ImpactNormal.Z < 0.55f)
	{
		ShowAnchorMessage(TEXT("Teleport blocked"));
		return false;
	}

	const FVector CandidateLocation = GroundHit.ImpactPoint + FVector::UpVector * (CapsuleHalfHeight + 3.0f);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	// Same rough shape as the player capsule, just checking the spot before moving.
	if (GetWorld()->OverlapBlockingTestByChannel(CandidateLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, QueryParams))
	{
		ShowAnchorMessage(TEXT("Teleport blocked"));
		return false;
	}

	OutLocation = CandidateLocation;
	return true;
}

void AMGP_2526Character::ShowAnchorMessage(const FString& Message) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow, Message);
	}
}

void AMGP_2526Character::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMGP_2526Character::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMGP_2526Character::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AMGP_2526Character::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
