// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZWidgetLayout.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

namespace ZWidgetLayout
{
	UTextBlock* MakeText(UWidgetTree& Tree, FName Name, int32 FontSize, const FLinearColor& Color, bool bBold, bool bWrap)
	{
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(bWrap);
		return Text;
	}

	UBorder* MakeBorder(UWidgetTree& Tree, FName Name, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
	}

	UButton* MakeButton(UWidgetTree& Tree, FName Name, const FText& Label, int32 FontSize, UTextBlock** OutLabel)
	{
		UButton* Button = Tree.ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(ButtonColor);

		UTextBlock* LabelText = MakeText(Tree, *(Name.ToString() + TEXT("Label")), FontSize, TextColor);
		LabelText->SetText(Label);
		LabelText->SetJustification(ETextJustify::Center);

		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(LabelText)))
		{
			ButtonSlot->SetPadding(FMargin(10.f, 4.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		if (OutLabel)
		{
			*OutLabel = LabelText;
		}

		return Button;
	}

	UProgressBar* MakeProgressBar(UWidgetTree& Tree, FName Name, const FLinearColor& FillColor)
	{
		UProgressBar* ProgressBar = Tree.ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		ProgressBar->SetFillColorAndOpacity(FillColor);
		ProgressBar->SetPercent(0.f);
		return ProgressBar;
	}

	USizeBox* MakeSizeBox(UWidgetTree& Tree, FName Name, float Width, float Height)
	{
		USizeBox* SizeBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		if (Width > 0.f)
		{
			SizeBox->SetWidthOverride(Width);
		}

		if (Height > 0.f)
		{
			SizeBox->SetHeightOverride(Height);
		}

		return SizeBox;
	}

	UVerticalBox* MakeVerticalBox(UWidgetTree& Tree, FName Name)
	{
		return Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
	}

	UHorizontalBox* MakeHorizontalBox(UWidgetTree& Tree, FName Name)
	{
		return Tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
	}

	UWrapBox* MakeWrapBox(UWidgetTree& Tree, FName Name, float Spacing)
	{
		UWrapBox* WrapBox = Tree.ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), Name);
		WrapBox->SetInnerSlotPadding(FVector2D(Spacing, Spacing));
		return WrapBox;
	}

	UScrollBox* MakeScrollBox(UWidgetTree& Tree, FName Name)
	{
		UScrollBox* ScrollBox = Tree.ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), Name);
		ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
		return ScrollBox;
	}

	void AddToVerticalBox(UVerticalBox* Box, UWidget* Child, const FMargin& Padding, bool bFill, EHorizontalAlignment HorizontalAlignment)
	{
		if (UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(Child))
		{
			BoxSlot->SetPadding(Padding);
			BoxSlot->SetHorizontalAlignment(HorizontalAlignment);
			BoxSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		}
	}

	void AddToHorizontalBox(UHorizontalBox* Box, UWidget* Child, const FMargin& Padding, float FillWeight, EVerticalAlignment VerticalAlignment)
	{
		if (UHorizontalBoxSlot* BoxSlot = Box->AddChildToHorizontalBox(Child))
		{
			BoxSlot->SetPadding(Padding);
			BoxSlot->SetVerticalAlignment(VerticalAlignment);

			FSlateChildSize Size(FillWeight > 0.f ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
			Size.Value = FillWeight > 0.f ? FillWeight : 1.f;
			BoxSlot->SetSize(Size);
		}
	}

	void AddToPanel(UPanelWidget* Panel, UWidget* Child, const FMargin& Padding)
	{
		UPanelSlot* PanelSlot = Panel->AddChild(Child);
		if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(PanelSlot))
		{
			WrapSlot->SetPadding(Padding);
		}
		else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(PanelSlot))
		{
			VerticalSlot->SetPadding(Padding);
		}
		else if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
		{
			ScrollSlot->SetPadding(Padding);
		}
		else if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
		{
			HorizontalSlot->SetPadding(Padding);
		}
	}

	void SetTextColor(UTextBlock* Text, const FLinearColor& Color)
	{
		if (Text)
		{
			Text->SetColorAndOpacity(FSlateColor(Color));
		}
	}
}
