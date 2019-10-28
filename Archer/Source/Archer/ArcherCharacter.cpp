// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ArcherCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Projectile.h"
#include "Kismet/KismetMathLibrary.h"
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

	// Set default value;
	bool bIsWeaponEquipped = false;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;	   

	// Set Default values for camera smooth zoom in and out (change to aim mode)	
	CameraMovementAlpha = 0.1f;

	DefaultTargetArmLength = 300.0f;
	DefaultFieldOfView = 90.0f;
	DefaultCameraBoomSocketOffsetY = 0.0f;

	AimModeTargetArmLength = 200.0f;
	AimModeFieldOfView = 70.0f;	
	AimModeCameraBoomSocketOffsetY = 60.0f;
	
	// Setting up default values for character movement
	SprintSpeedCrouched = 187.5f;
	RunSpeed = 375.f;
	SprintSpeed = 562.5f;
	WalkSpeed = 180.5f;  //  93.75f;
	WalkSpeedCrouched = 46.87f;	
	bWalkModeActive = false;
	JumpWalkZVelocity = 375.f;
	JumpRunZVelocity = 450.f;
	JumpSprintZVelocity = 562.5f;	
	bIsSprintingAllowed = true;	

	MaxUpperBodyRotation = 90.0f;
	
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

	BowGripOffset = CreateDefaultSubobject<USceneComponent>(TEXT("BowGripOffset"));
	BowGripOffset->SetupAttachment(GetMesh());

	// Create Weapon tatic mesh component and make it hidden in game
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));	
	WeaponMesh->SetupAttachment(BowGripOffset);
	
	WeaponMesh->SetHiddenInGame(true, true);

	// Create point at whitch projectiles will be spawned (shoot from)
	// TODO change attachment to the bow when separate static mesh for weapon is made.
	ProjectileReleasePoint = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileReleasePoint"));
	ProjectileReleasePoint->SetupAttachment(WeaponMesh);
	
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

	BowGripOffset->AttachToComponent(GetMesh(), FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("LeftHandGripPoint"));
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

	PlayerInputComponent->BindAction("EquipWeapon", IE_Pressed, this, &AArcherCharacter::EquipWeapon);

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


void AArcherCharacter::EquipWeapon()
{
	if (!bIsWeaponEquipped)
	{
		if (PlayMontageAnimation(EquipWeaponMontage, false))
		{
			bIsWeaponEquipped = true;			
		}		
	}
	else if (bIsWeaponEquipped)
	{		
		if (PlayMontageAnimation(DisarmWeaponMontage, false))
		{
			bIsWeaponEquipped = false;			
		}				
	}	
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

			ProjectileMesh->SetHiddenInGame(true, true);
			bIsArrowLoaded = false;

			ProjectileMesh->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
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
	if (bIsWeaponEquipped && ProjectileClass != NULL)
	{
		bIsAiming = true;

		// Adjust movement settings while aiming
		GetCharacterMovement()->SetJumpAllowed(false);
		if (!bWalkModeActive)
		{
			ToggleWalkMode();
			bWasWalkModeChanged = true;
		}

		// Play Drawing arrow animation if needed
		if (!bIsArrowLoaded)
		{
			/**bIsArrowLoaded will be change to true (in BP animinstance) after draw arrow montage was played (notify) */
			PlayMontageAnimation(DrawArrowMontage, false);
		}
	}	
}

void AArcherCharacter::StopAiming()
{
	bIsAiming = false;

	// Set movement settings back to normal
	GetCharacterMovement()->SetJumpAllowed(true);
	if (bWasWalkModeChanged)
	{
		ToggleWalkMode();
		bWasWalkModeChanged = false;
	}		

	// Play Drawing arrow animation if needed
	if (bIsArrowLoaded && bIsWeaponEquipped)
	{
		/**bIsArrowLoaded will be change to false (in BP animinstance) after draw arrow montage was played (notify) */
		PlayMontageAnimation(DrawArrowMontage, true);
	}
	else
	{
		PlayMontageAnimation(DrawArrowLoopSectionMontage, true);
	}
	// TODO Add timer to wait for animation to stop playing so that it bCanAim will be change back to true 
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
