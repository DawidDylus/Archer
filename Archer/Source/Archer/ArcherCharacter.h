// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArcherCharacter.generated.h"

UCLASS(config=Game)
class AArcherCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;		

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile, meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* ProjectileMesh;	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* ProjectileReleasePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* BowGripOffset;

public:
	AArcherCharacter();	

protected:
	virtual void BeginPlay();
		
public:

	/** Projectile class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	TSubclassOf<class AProjectile> ProjectileClass;	

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;	

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float RunSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float SprintSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float SprintSpeedCrouched;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float WalkSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float WalkSpeedCrouched;	

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float JumpWalkZVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float JumpRunZVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Character)
		float JumpSprintZVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Character)
		bool bIsAiming;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Character)
		float MaxUpperBodyRotation;	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Character)
		bool bIsWeaponEquipped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Character)
		bool bIsArrowLoaded;	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float CameraMovementAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float DefaultTargetArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float DefaultFieldOfView;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float DefaultCameraBoomSocketOffsetY;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float AimModeTargetArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float AimModeFieldOfView;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim Camera / Smooth Zoom")
		float AimModeCameraBoomSocketOffsetY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Montage Animations")
		class UAnimMontage* DrawArrowLoopSectionMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Montage Animations")
		class UAnimMontage* DrawArrowMontage;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Montage Animations")
		class UAnimMontage* EquipWeaponMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Montage Animations")
		class UAnimMontage* DisarmWeaponMontage;
	

protected:			

	bool bWalkModeActive;	
	
	bool bIsSprintingAllowed;	

	//** Attach and make visible mesh of a weapon to character*/
	void EquipWeapon();

	//** Spawn ProjectileClas, works only if bIsLoaded = true*/
	void Shoot();
	
	//** Turn On/Off WalkMode (Change MaxWalkingSpeed) */
	void ToggleWalkMode();	

	//** Change MaxWalkingSpeed of MovementComponent to SprintSpeed */
	void Sprint();
	
	//** Depending on movement mode (run/walk) Change MaxWalkingSpeed of Movementcomponent */
	void StopSprinting();

	/**
	 * Play specified animation
	 * @param AnimationToPlay - Pointer to AnimationMontage that will be played
	 * @param bPlayInReverse - Should or should not play animation in reverse
	 * @return true if animation was played correctly.
	 */
	bool PlayMontageAnimation(class UAnimMontage* AnimationToPlay, const bool bPlayInReverse);
	
	//** Change Field of View to zoom, UseControllerRotationPitch to true */
	void Aim();
	
	void StopAiming();
	
	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	//** Returns ProjectileMesh subobject **/
	FORCEINLINE class UStaticMeshComponent* GetProjectileMesh() const { return ProjectileMesh; }
	//** Returns ProjectileReleasePoint subobject **/
	FORCEINLINE	class USceneComponent* GetProjectileReleasePoint() const { return ProjectileReleasePoint; }
	//** Returns WeaponMesh subobject **/
	FORCEINLINE class  UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	//** Returns BowGripOffset subobject**/
	FORCEINLINE  class USceneComponent* GetBowGripOffset() const { return BowGripOffset; }	
};

