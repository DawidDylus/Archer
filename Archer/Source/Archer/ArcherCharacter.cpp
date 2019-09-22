// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ArcherCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Projectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Public/TimerManager.h"

//////////////////////////////////////////////////////////////////////////
// AArcherCharacter

AArcherCharacter::AArcherCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// check if arrow is ready to be shoot
	bIsArrowLoaded = false;

	// Set default value for aiming mode;
	bIsAiming = false;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Setting up default values for character movement
	SprintSpeedCrouched = 187.5f;
	RunSpeed = 375.f;
	SprintSpeed = 562.5f;
	WalkSpeed = 93.75f;
	WalkSpeedCrouched = 46.87f;	
	bWalkModeActive = false;
	JumpWalkZVelocity = 375.f;
	JumpRunZVelocity = 450.f;
	JumpSprintZVelocity = 562.5f;	
	bIsSprintingAllowed = true;	
	
	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = JumpRunZVelocity;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = WalkSpeedCrouched;	

	// Create projectile static mesh component to use it in animation that require seeing a projectile
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));	
	ProjectileMesh->SetHiddenInGame(true, true);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DefaultProjectileLocation = FVector(0.0f, -4.0f, 30.0f);	
	DefaultProjectileRotation = FRotator(0.0f, 30.0f, -100.0f);
	ProjectileMesh->SetRelativeLocationAndRotation(DefaultProjectileLocation, DefaultProjectileRotation);

	// Create point at whitch projectiles will be spawned (shoot from)
	ProjectileReleasePoint = CreateEditorOnlyDefaultSubobject<USceneComponent>(TEXT("ProjectileReleasePoint"));
	ProjectileReleasePoint->SetupAttachment(GetMesh());
	
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void AArcherCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach projectile mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	ProjectileMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("RightHandGripPoint"));
}

//////////////////////////////////////////////////////////////////////////
// Input

void AArcherCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AArcherCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AArcherCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AArcherCharacter::MoveRight);

	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AArcherCharacter::Sprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AArcherCharacter::StopSprinting);

	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AArcherCharacter::Aim);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AArcherCharacter::StopAiming);

	PlayerInputComponent->BindAction("Shoot", IE_Pressed, this, &AArcherCharacter::Shoot);

	PlayerInputComponent->BindAction("WalkMode", IE_Pressed, this, &AArcherCharacter::ToggleWalkMode);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AArcherCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AArcherCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AArcherCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AArcherCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AArcherCharacter::OnResetVR);
}


void AArcherCharacter::Shoot()
{
	if (bIsAiming && bIsArrowLoaded && ProjectileClass != NULL)
	{
		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

			FRotator SpawnRotation = Controller->GetControlRotation();
			FVector SpawnLocation = ProjectileReleasePoint->GetComponentLocation();
			

			World->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);				
		}		
	}	
}

void AArcherCharacter::ToggleWalkMode()
{
	if (!bWalkModeActive)
	{
		bWalkModeActive = true;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		bIsSprintingAllowed = false;
	}
	else if (bWalkModeActive)
	{
		bWalkModeActive = false;
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
		bIsSprintingAllowed = true;
	}
}

void AArcherCharacter::Sprint()
{
	if (bIsSprintingAllowed)
	{
		if (!bWalkModeActive)
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
			GetCharacterMovement()->JumpZVelocity = JumpSprintZVelocity;			
		}
		else if (bWalkModeActive)
		{
			GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
			GetCharacterMovement()->JumpZVelocity = JumpRunZVelocity;
		}
	}
}

void AArcherCharacter::StopSprinting()
{
	if (!bWalkModeActive)
	{
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
		GetCharacterMovement()->JumpZVelocity = JumpRunZVelocity;		
	}
	else if (bWalkModeActive)
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		GetCharacterMovement()->JumpZVelocity = JumpWalkZVelocity;		
	}
}

bool AArcherCharacter::PlayMontageAnimation(UAnimMontage* AnimationToPlay, const bool bPlayInReverse)
{
	// try to play arraw drawing animation if specified
	if (AnimationToPlay != NULL)
	{		
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL)
		{			
			if (!bPlayInReverse)
			{
				AnimInstance->Montage_Play(AnimationToPlay, 1.0f);
			}
			else if (bPlayInReverse)
			{
				AnimInstance->Montage_Play(AnimationToPlay, -1.0f, EMontagePlayReturnType::MontageLength, 1.0f);
			}	
			return true;
		}		
	}	
	return false;	
}


// Help to return to walk mode that was set befor aim function.
bool bWasWalkModeChanged = false;
void AArcherCharacter::Aim()
{
	bIsAiming = true;

	// Adjust camera to better fit aiming 
	bUseControllerRotationYaw = true;		
	FollowCamera->FieldOfView = 70;
	
	// Adjust movement settings while aiming
	GetCharacterMovement()->SetJumpAllowed(false);
	if (!bWalkModeActive)
	{		
		ToggleWalkMode();
		bWasWalkModeChanged = true;		
	}		
	
	// Play Drawing arrow animation if needed
	if (!bIsArrowLoaded && ProjectileClass != NULL)
	{
		if (PlayMontageAnimation(DrawArrowAnimation, false))
		{
			bIsArrowLoaded = true;			
		}		
	}
}

void AArcherCharacter::StopAiming()
{	
	bIsAiming = false;

	// Set camera options back to normal
	bUseControllerRotationYaw = false;
	FollowCamera->FieldOfView = 90;

	// Set movement settings back to normal
	GetCharacterMovement()->SetJumpAllowed(true);
	if (bWasWalkModeChanged)
	{		
		ToggleWalkMode();
		bWasWalkModeChanged = false;
	}

	// Play Drawing arrow animation (in reverse) if needed
	if (bIsArrowLoaded)
	{
		if (PlayMontageAnimation(DrawArrowAnimation, true))
		{
			bIsArrowLoaded = false;
			// TODO Destroy arrow on bow. 	
		}
	}	
}

void AArcherCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AArcherCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AArcherCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AArcherCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AArcherCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AArcherCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AArcherCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
