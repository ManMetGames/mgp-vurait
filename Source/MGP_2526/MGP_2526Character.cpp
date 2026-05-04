// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGP_2526Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
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

void AMGP_2526Character::BeginPlay()
{
	Super::BeginPlay();

	LastCheckpointLocation = GetActorLocation();
	LastCheckpointRotation = GetActorRotation();
}

void AMGP_2526Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCheckpointAndHazards();
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
	FVector SpawnLocation;
	if (!FindAnchorSpawnLocation(CameraDirection, HandOffset, SpawnLocation))
	{
		ShowAnchorMessage(TEXT("Too close"));
		return;
	}

	const FRotator SpawnRotation = ThrowDirection.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

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

bool AMGP_2526Character::FindAnchorSpawnLocation(const FVector& CameraDirection, const FVector& HandOffset, FVector& OutLocation) const
{
	if (!FollowCamera || !GetWorld())
	{
		return false;
	}

	const FVector CameraLocation = FollowCamera->GetComponentLocation();
	const FVector WantedLocation = CameraLocation + CameraDirection * AnchorSpawnDistance + HandOffset;
	const FCollisionShape AnchorShape = FCollisionShape::MakeSphere(20.0f);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AnchorSpawnCheck), false, this);
	if (ActiveAnchor)
	{
		QueryParams.AddIgnoredActor(ActiveAnchor);
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult SpawnHit;
	const bool bBlocked = GetWorld()->SweepSingleByObjectType(
		SpawnHit,
		CameraLocation,
		WantedLocation,
		FQuat::Identity,
		ObjectParams,
		AnchorShape,
		QueryParams);

	if (bBlocked)
	{
		return false;
	}

	// Final overlap check, just in case the sweep starts too close to the surface.
	if (GetWorld()->OverlapAnyTestByObjectType(WantedLocation, FQuat::Identity, ObjectParams, AnchorShape, QueryParams))
	{
		return false;
	}

	OutLocation = WantedLocation;
	return true;
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

	if (!ActiveAnchor->IsValidSurface())
	{
		ShowAnchorMessage(TEXT("Invalid surface"));
		return;
	}

	FVector TeleportLocation;
	if (!FindSafeTeleportLocation(TeleportLocation))
	{
		return;
	}

	if (!TeleportTo(TeleportLocation, GetActorRotation(), false, false))
	{
		ShowAnchorMessage(TEXT("Teleport blocked"));
		return;
	}

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
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius() + 3.0f;
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 8.0f;
	const FVector SurfacePoint = ActiveAnchor->GetSurfacePoint();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AnchorTeleportCheck), false, this);
	QueryParams.AddIgnoredActor(ActiveAnchor);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

	FVector DesiredLocation = SurfacePoint + SurfaceNormal * (CapsuleRadius + 12.0f);
	if (SurfaceNormal.Z > 0.5f)
	{
		DesiredLocation = SurfacePoint + FVector::UpVector * (CapsuleHalfHeight + 6.0f);
	}
	else if (SurfaceNormal.Z < -0.35f)
	{
		// Ceiling case: centre the capsule below the hit point so the top just clears it.
		DesiredLocation = SurfacePoint - FVector::UpVector * (CapsuleHalfHeight + 6.0f);
	}

	FVector SideVector = FVector::CrossProduct(SurfaceNormal, FVector::UpVector).GetSafeNormal();
	if (SideVector.IsNearlyZero())
	{
		SideVector = GetActorRightVector();
	}

	const FVector UpVector = FVector::UpVector;
	const float StepSize = 45.0f;
	TArray<FVector> Offsets = {
		FVector::ZeroVector,
		SideVector * StepSize,
		-SideVector * StepSize,
	};

	if (SurfaceNormal.Z < -0.35f)
	{
		// Ceiling anchors only search below the ceiling.
		Offsets.Add(-UpVector * StepSize);
		Offsets.Add(-UpVector * StepSize * 2.0f);
		Offsets.Add(SideVector * StepSize - UpVector * StepSize);
		Offsets.Add(-SideVector * StepSize - UpVector * StepSize);
	}
	else
	{
		Offsets.Add(SurfaceNormal * StepSize);
		Offsets.Add(SurfaceNormal * StepSize * 2.0f);
		Offsets.Add(UpVector * StepSize);
		Offsets.Add(-UpVector * StepSize);
		Offsets.Add(SurfaceNormal * StepSize + UpVector * StepSize);
		Offsets.Add(SurfaceNormal * StepSize - UpVector * StepSize);
		Offsets.Add(SurfaceNormal * StepSize + SideVector * StepSize);
		Offsets.Add(SurfaceNormal * StepSize - SideVector * StepSize);
	}

	for (const FVector& Offset : Offsets)
	{
		const FVector CandidateLocation = DesiredLocation + Offset;

		// Tries a few nearby spots instead of failing on the first blocked point.
		if (IsTeleportSpotClear(CandidateLocation, CapsuleShape, QueryParams))
		{
			OutLocation = CandidateLocation;
			return true;
		}
	}

	ShowAnchorMessage(TEXT("Teleport blocked"));
	return false;
}

bool AMGP_2526Character::IsTeleportSpotClear(const FVector& Location, const FCollisionShape& CapsuleShape, const FCollisionQueryParams& QueryParams) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Checks against geometry directly, not just the pawn collision profile.
	if (IsGeometryOverlapping(Location, CapsuleShape, QueryParams))
	{
		return false;
	}

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius() + 3.0f;
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 8.0f;
	const FVector HeadLocation = Location + FVector::UpVector * (CapsuleHalfHeight - CapsuleRadius);
	const FCollisionShape HeadShape = FCollisionShape::MakeSphere(CapsuleRadius);

	// Extra head check for ceiling cases while jumping.
	if (IsGeometryOverlapping(HeadLocation, HeadShape, QueryParams))
	{
		return false;
	}

	// No path sweep here, since teleporting should not care about the space between points.
	return true;
}

bool AMGP_2526Character::IsGeometryOverlapping(const FVector& Location, const FCollisionShape& Shape, const FCollisionQueryParams& QueryParams) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	return World->OverlapAnyTestByObjectType(Location, FQuat::Identity, ObjectParams, Shape, QueryParams);
}

void AMGP_2526Character::UpdateCheckpointAndHazards()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector ActorLocation = GetActorLocation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AnchorLevelBlocks), false, this);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	World->OverlapMultiByObjectType(Overlaps, ActorLocation, FQuat::Identity, ObjectParams, CapsuleShape, QueryParams);

	// Main kill check for bricks the player is actually touching.
	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* HitActor = Result.GetActor();
		if (HitActor && HitActor->ActorHasTag(TEXT("KillBrick")))
		{
			ResetToCheckpoint();
			return;
		}
	}

	FHitResult FloorHit;
	const FVector TraceStart = ActorLocation;
	const FVector TraceEnd = ActorLocation - FVector::UpVector * (CapsuleHalfHeight + 35.0f);

	// This catches thin floors better than only using the capsule overlap.
	World->LineTraceSingleByObjectType(FloorHit, TraceStart, TraceEnd, ObjectParams, QueryParams);
	AActor* FloorActor = FloorHit.GetActor();
	if (!FloorActor)
	{
		return;
	}

	if (FloorActor->ActorHasTag(TEXT("KillBrick")))
	{
		ResetToCheckpoint();
		return;
	}

	if (FloorActor->ActorHasTag(TEXT("Checkpoint")))
	{
		SaveCheckpoint(FloorActor);
	}
}

void AMGP_2526Character::SaveCheckpoint(AActor* CheckpointActor)
{
	if (!CheckpointActor || CurrentCheckpointActor == CheckpointActor)
	{
		return;
	}

	// Same checkpoint should not keep spamming the message.
	CurrentCheckpointActor = CheckpointActor;

	FVector CheckpointCentre;
	FVector CheckpointExtent;
	CheckpointActor->GetActorBounds(false, CheckpointCentre, CheckpointExtent);

	// Respawn from the middle of the checkpoint, not where the player clipped it.
	LastCheckpointLocation = CheckpointCentre;
	LastCheckpointLocation.Z = CheckpointCentre.Z + CheckpointExtent.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.0f;
	LastCheckpointRotation = GetActorRotation();
	ShowAnchorMessage(TEXT("Checkpoint"));
}

void AMGP_2526Character::ResetToCheckpoint()
{
	if (ActiveAnchor)
	{
		ActiveAnchor->Destroy();
		ActiveAnchor = nullptr;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	TeleportTo(LastCheckpointLocation, LastCheckpointRotation, false, false);
	ShowAnchorMessage(TEXT("Reset"));
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
