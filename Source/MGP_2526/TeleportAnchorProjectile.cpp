// Copyright Epic Games, Inc. All Rights Reserved.

#include "TeleportAnchorProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ATeleportAnchorProjectile::ATeleportAnchorProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(18.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	// Stops it catching on the player or camera straight after being thrown.
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->BodyInstance.bUseCCD = true;
	CollisionComponent->OnComponentHit.AddDynamic(this, &ATeleportAnchorProjectile::HandleHit);
	SetRootComponent(CollisionComponent);

	AnchorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnchorMesh"));
	AnchorMesh->SetupAttachment(CollisionComponent);
	AnchorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnchorMesh->SetRelativeScale3D(FVector(0.36f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		// Engine sphere is enough for the prototype.
		AnchorMesh->SetStaticMesh(SphereMesh.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 3200.0f;
	ProjectileMovement->ProjectileGravityScale = 0.35f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bForceSubStepping = true;
	ProjectileMovement->MaxSimulationTimeStep = 0.016f;
	ProjectileMovement->MaxSimulationIterations = 8;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;
}

void ATeleportAnchorProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent && GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}
}

void ATeleportAnchorProjectile::LaunchAnchor(const FVector& Direction, float Speed, float GravityScale)
{
	if (!ProjectileMovement)
	{
		return;
	}

	const FVector SafeDirection = Direction.GetSafeNormal();
	ProjectileMovement->ProjectileGravityScale = GravityScale;
	ProjectileMovement->Velocity = SafeDirection * Speed;
	// Movement is started manually so the character can choose the throw type.
	ProjectileMovement->Activate(true);
}

void ATeleportAnchorProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasLanded || OtherActor == this)
	{
		return;
	}

	bHasLanded = true;

	// Pushing it off the wall stops the mesh sinking into the surface.
	if (ProjectileMovement)
	{
		// Leave it where it lands.
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	SurfacePoint = Hit.ImpactPoint;
	SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
	SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * 18.0f);
	SetActorRotation(Hit.ImpactNormal.Rotation());
}
