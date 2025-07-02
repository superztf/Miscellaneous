
/*
 * 2025.07.02
 * 由于UE5.6.0版本DistanceCurveModifier功能出现故障，提供此替代方案
 * 注：当前编译至少添加这3个模块依赖： AnimationModifiers   AnimationModifierLibrary   AnimationBlueprintLibrary
 */

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "MotionExtractorTypes.h"
#include "EastingDistanceCurve.generated.h"


/** Extracts traveling distance information from root motion and bakes it to a curve.
 * A negative value indicates distance remaining to a stop or pivot point.
 * A positive value indicates distance traveled from a start point or from the beginning of the clip.
 */
UCLASS()
class UEastingDistanceCurve : public UAnimationModifier
{
	GENERATED_BODY()

public:
	/** Bone we are going to generate the curve from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName BoneName = FName(TEXT("root"));

	/** Rate used to sample the animation. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin = "1"))
	int32 SampleRate = 30;

	/** Name for the generated curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FName CurveName = "Distance";

	/** Root motion speed must be below this threshold to be considered stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(EditCondition="!bStopAtEnd"))
	float StopSpeedThreshold = 5.0f;

	/** Axes to calculate the distance value from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EMotionExtractor_Axis Axis = EMotionExtractor_Axis::XY;

	/** Root motion is considered to be stopped at the clip's end */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bStopAtEnd = false;

	virtual void OnApply_Implementation(UAnimSequence* Animation) override;
	virtual void OnRevert_Implementation(UAnimSequence* Animation) override;

private:

	/** Helper functions to calculate the magnitude of a vector only considering a specific axis or axes */
	static float CalculateMagnitude(const FVector& Vector, EMotionExtractor_Axis Axis);
	static float CalculateMagnitudeSq(const FVector& Vector, EMotionExtractor_Axis Axis);
};