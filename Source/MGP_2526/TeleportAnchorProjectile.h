// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TeleportAnchorProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class MGP_2526_API ATeleportAnchorProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATeleportAnchorProjectile();

	void LaunchAnchor(const FVector& Direction, float Speed, float GravityScale);
	bool HasLanded() const { return bHasLanded; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Anchor")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Anchor")
	TObjectPtr<UStaticMeshComponent> AnchorMesh;

	UPROPERTY(VisibleAnywhere, Category = "Anchor")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	bool bHasLanded = false;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
