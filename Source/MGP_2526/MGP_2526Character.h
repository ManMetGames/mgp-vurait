// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MGP_2526Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UMaterialInterface;
class ATeleportAnchorProjectile;
struct FInputActionValue;

struct FActivatorSection
{
	FString SectionPath;
	TArray<TWeakObjectPtr<AActor>> Bricks;
	int32 ActiveIndex = INDEX_NONE;
	float TimeWithoutPrevious = 0.0f;
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMGP_2526Character : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category="Teleport Anchor")
	TSubclassOf<ATeleportAnchorProjectile> AnchorProjectileClass;

	UPROPERTY(EditAnywhere, Category="Teleport Anchor", meta = (ClampMin = "0"))
	float AnchorSpawnDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category="Teleport Anchor", meta = (ClampMin = "100"))
	float StraightThrowSpeed = 2800.0f;

	UPROPERTY(EditAnywhere, Category="Teleport Anchor", meta = (ClampMin = "100"))
	float LobThrowSpeed = 1900.0f;

	UPROPERTY(EditAnywhere, Category="Teleport Anchor", meta = (ClampMin = "100"))
	float MaxTeleportDistance = 2200.0f;

public:

	/** Constructor */
	AMGP_2526Character();	

protected:

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void ThrowStraightAnchor();

	void ThrowLobAnchor();

	void ThrowAnchor(float Speed, float UpwardsAim, float GravityScale);

	bool FindAnchorSpawnLocation(const FVector& CameraDirection, const FVector& HandOffset, FVector& OutLocation) const;

	void TeleportToAnchor();

	bool FindSafeTeleportLocation(FVector& OutLocation) const;

	bool IsTeleportSpotClear(const FVector& Location, const FCollisionShape& CapsuleShape, const FCollisionQueryParams& QueryParams) const;

	bool IsGeometryOverlapping(const FVector& Location, const FCollisionShape& Shape, const FCollisionQueryParams& QueryParams) const;

	void UpdateCheckpointAndHazards();

	void SaveCheckpoint(AActor* CheckpointActor);

	void ResetToCheckpoint();

	void SetupActivatorBricks();

	void UpdateActivatorAnchor();

	void UpdateActivatorTimeouts(float DeltaSeconds);

	void HandleActivatorAnchor(AActor* ActivatorActor);

	void StartActivatorReleaseDelay();

	void SetActivatorBrickState(AActor* Brick, bool bActive);

	void DeactivateActivatorBrick(AActor* Brick);

	void ShowAnchorMessage(const FString& Message) const;

	UPROPERTY()
	TObjectPtr<ATeleportAnchorProjectile> ActiveAnchor;

	UPROPERTY()
	TObjectPtr<AActor> CurrentCheckpointActor;

	FVector LastCheckpointLocation = FVector::ZeroVector;
	FRotator LastCheckpointRotation = FRotator::ZeroRotator;

	TArray<FActivatorSection> ActivatorSections;

	UPROPERTY()
	TObjectPtr<ATeleportAnchorProjectile> LastProcessedActivatorAnchor;

	TWeakObjectPtr<AActor> ActivatorBrickHeldByAnchor;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ActivatorActiveMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ActivatorInactiveMaterial;

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

