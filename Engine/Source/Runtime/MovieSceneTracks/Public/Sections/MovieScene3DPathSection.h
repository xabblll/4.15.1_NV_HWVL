// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene3DConstraintSection.h"
#include "MovieScene3DPathSection.generated.h"

class USplineComponent;

UENUM(BlueprintType)
enum class MovieScene3DPathSection_Axis : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z"),
	NEG_X UMETA(DisplayName = "-X"),
	NEG_Y UMETA(DisplayName = "-Y"),
	NEG_Z UMETA(DisplayName = "-Z")
};

/**
 * A 3D Path section
 */
UCLASS(MinimalAPI)
class UMovieScene3DPathSection
	: public UMovieScene3DConstraintSection
{
	GENERATED_UCLASS_BODY()

public:

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;

	/**
	 * Evaluates the path track
	 *
	 * @param Time				The position in time within the movie scene
	 */
	void Eval( USceneComponent* SceneComponent, float Time, USplineComponent* SplineComponent, FVector& OutTranslation, FRotator& OutRotation ) const;

	/** 
	 * Adds a path to the section
	 *
	 * @param Time	The location in time where the path should be added
	 * @param SequenceEndTime   The time at the end of the sequence, by default the path is set to end at this time
	 * @param InPathId The id to the path
	 */
	void AddPath( float Time, float SequenceEndTime, const FGuid& InPathId);

	/** 
	 * Returns the timing curve
	 *
	 * @return The timing curve
	 */
	MOVIESCENETRACKS_API FRichCurve& GetTimingCurve() { return TimingCurve; }
	
private:
	/** Timing Curve */
	UPROPERTY(EditAnywhere, Category="Path")
	FRichCurve TimingCurve;

	/** Front Axis */
	UPROPERTY(EditAnywhere, Category="Path")
	MovieScene3DPathSection_Axis FrontAxisEnum;

	/** Up Axis */
	UPROPERTY(EditAnywhere, Category="Path")
	MovieScene3DPathSection_Axis UpAxisEnum;

	/** Follow Curve */
	UPROPERTY(EditAnywhere, Category="Path")
	uint32 bFollow:1;

	/** Reverse Timing */
	UPROPERTY(EditAnywhere, Category="Path")
	uint32 bReverse:1;

	/** Force Upright */
	UPROPERTY(EditAnywhere, Category="Path")
	uint32 bForceUpright:1;
};