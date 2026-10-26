// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Types/SlateEnums.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UPanelWidget;
class UProgressBar;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UWidgetTree;
class UWrapBox;

/**
 *  Colors and helpers for the built-in looks of the inventory screen's widgets, used when their Widget Blueprint has no widgets of its own
 *  (a Widget Blueprint with its own widgets doesn't use any of this)
 */
namespace ZWidgetLayout
{
	// dark panels, bone-white text, worn gold for what's important
	inline const FLinearColor BackdropColor(0.f, 0.f, 0.f, 0.6f);
	inline const FLinearColor PanelColor(0.01f, 0.01f, 0.012f, 0.97f);
	inline const FLinearColor SectionColor(0.024f, 0.024f, 0.028f, 1.f);
	inline const FLinearColor SlotColor(0.045f, 0.045f, 0.052f, 1.f);
	inline const FLinearColor SlotHoveredColor(0.1f, 0.1f, 0.115f, 1.f);
	inline const FLinearColor SlotSelectedColor(0.34f, 0.25f, 0.09f, 1.f);
	inline const FLinearColor SlotDropTargetColor(0.12f, 0.3f, 0.15f, 1.f);
	inline const FLinearColor TextColor(0.86f, 0.82f, 0.74f, 1.f);
	inline const FLinearColor MutedTextColor(0.52f, 0.5f, 0.46f, 1.f);
	inline const FLinearColor AccentColor(0.92f, 0.7f, 0.32f, 1.f);
	inline const FLinearColor GoodColor(0.56f, 0.8f, 0.46f, 1.f);
	inline const FLinearColor ButtonColor(0.12f, 0.12f, 0.13f, 1.f);
	inline const FLinearColor PrimaryButtonColor(0.41f, 0.31f, 0.14f, 1.f);

	/** A text block with this size and color (FontSize in points) */
	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 FontSize, const FLinearColor& Color, bool bBold = false, bool bWrap = false);

	/** A colored box around one child */
	UBorder* MakeBorder(UWidgetTree& Tree, FName Name, const FLinearColor& Color, const FMargin& Padding);

	/** A dark button with a text label; OutLabel gets the label, e.g. to change it later */
	UButton* MakeButton(UWidgetTree& Tree, FName Name, const FText& Label, int32 FontSize = 12, UTextBlock** OutLabel = nullptr);

	UProgressBar* MakeProgressBar(UWidgetTree& Tree, FName Name, const FLinearColor& FillColor);

	/** A fixed size around one child (0 = no fixed size in that direction) */
	USizeBox* MakeSizeBox(UWidgetTree& Tree, FName Name, float Width, float Height);

	UVerticalBox* MakeVerticalBox(UWidgetTree& Tree, FName Name);

	UHorizontalBox* MakeHorizontalBox(UWidgetTree& Tree, FName Name);

	/** Children side by side, wrapping onto the next line, with Spacing between them */
	UWrapBox* MakeWrapBox(UWidgetTree& Tree, FName Name, float Spacing);

	UScrollBox* MakeScrollBox(UWidgetTree& Tree, FName Name);

	/** Adds to a vertical box; Fill takes the height that's left */
	void AddToVerticalBox(UVerticalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(0.f), bool bFill = false, EHorizontalAlignment HorizontalAlignment = HAlign_Fill);

	/** Adds to a horizontal box; FillWeight > 0 shares the width that's left by that weight */
	void AddToHorizontalBox(UHorizontalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(0.f), float FillWeight = 0.f, EVerticalAlignment VerticalAlignment = VAlign_Center);

	/** Adds a child to any panel (a list or a wrap box), with a little space around it if the panel's slots have padding */
	void AddToPanel(UPanelWidget* Panel, UWidget* Child, const FMargin& Padding = FMargin(0.f));

	/** Sets a text block's color (does nothing if it's null) */
	void SetTextColor(UTextBlock* Text, const FLinearColor& Color);
}
