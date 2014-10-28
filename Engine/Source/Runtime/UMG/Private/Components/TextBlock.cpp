// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UTextBlock

UTextBlock::UTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	WidgetStyle = *UTextBlock::GetDefaultStyle();

	Text = LOCTEXT("TextBlockDefaultValue", "Text Block");
	ShadowOffset = FVector2D(1.0f, 1.0f);
	ColorAndOpacity = FLinearColor::White;
	ShadowColorAndOpacity = FLinearColor::Transparent;
	LineHeightPercentage = 1.0f;

	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	Font = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
}

void UTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyTextBlock.Reset();
}

void UTextBlock::SetColorAndOpacity(FSlateColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetColorAndOpacity( InColorAndOpacity );
	}
}

void UTextBlock::SetShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity)
{
	ShadowColorAndOpacity = InShadowColorAndOpacity;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetShadowColorAndOpacity(InShadowColorAndOpacity);
	}
}

void UTextBlock::SetShadowOffset(FVector2D InShadowOffset)
{
	ShadowOffset = InShadowOffset;
	if( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetShadowOffset(ShadowOffset);
	}
}

TSharedRef<SWidget> UTextBlock::RebuildWidget()
{
	MyTextBlock = SNew(STextBlock)
		.TextStyle(&WidgetStyle);

	return MyTextBlock.ToSharedRef();
}

void UTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = OPTIONAL_BINDING(FText, Text);
	TAttribute<FSlateColor> ColorAndOpacityBinding = OPTIONAL_BINDING(FSlateColor, ColorAndOpacity);
	TAttribute<FLinearColor> ShadowColorAndOpacityBinding = OPTIONAL_BINDING(FLinearColor, ShadowColorAndOpacity);

	MyTextBlock->SetText(TextBinding);
	MyTextBlock->SetFont(Font);
	MyTextBlock->SetColorAndOpacity(ColorAndOpacityBinding);
	MyTextBlock->SetShadowOffset(ShadowOffset);
	MyTextBlock->SetShadowColorAndOpacity(ShadowColorAndOpacityBinding);
	MyTextBlock->SetAutoWrapText(AutoWrapText);
	MyTextBlock->SetWrapTextAt(WrapTextAt != 0 ? WrapTextAt : TAttribute<float>());
	MyTextBlock->SetMinDesiredWidth(MinDesiredWidth);
	MyTextBlock->SetLineHeightPercentage(LineHeightPercentage);
	MyTextBlock->SetMargin(Margin);
	MyTextBlock->SetJustification(Justification);
}

FText UTextBlock::GetText() const
{
	if (MyTextBlock.IsValid())
	{
		return MyTextBlock->GetText();
	}

	return Text;
}

void UTextBlock::SetText(FText InText)
{
	Text = InText;
	if ( MyTextBlock.IsValid() )
	{
		MyTextBlock->SetText(Text);
	}
}

void UTextBlock::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FTextBlockStyle* StylePtr = Style_DEPRECATED->GetStyle<FTextBlockStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}
}

const FTextBlockStyle* UTextBlock::GetDefaultStyle()
{
	static const STextBlock::FArguments Defaults;
	return Defaults._TextStyle;
}

#if WITH_EDITOR

FString UTextBlock::GetLabelMetadata() const
{
	const int32 MaxSampleLength = 15;

	FString TextStr = Text.ToString();
	TextStr = TextStr.Len() <= MaxSampleLength ? TextStr : TextStr.Left(MaxSampleLength - 2) + TEXT("..");
	return TEXT(" \"") + TextStr + TEXT("\"");
}

void UTextBlock::HandleTextCommitted(const FText& InText, ETextCommit::Type CommitteType)
{
	//TODO UMG How will this migrate to the template?  Seems to me we need the previews to have access to their templates!
	//TODO UMG How will the user click the editable area?  There is an overlay blocking input so that other widgets don't get them.
	//     Need a way to recognize one particular widget and forward things to them!
}

const FSlateBrush* UTextBlock::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.TextBlock");
}

const FText UTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
