// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ZEquipmentComponent.h"
#include "ZEquipmentPanelWidget.generated.h"

class UAbilitySystemComponent;
class UTextBlock;
class UZInventoryScreenWidget;
class UZItemInstance;
class UZItemSlotWidget;
struct FOnAttributeChangeData;

/**
 *  The equipment window: a slot for each weapon, piece of armor and piece of jewelry the player can equip (Project Settings → Game → Equipment), and their armor
 *  Part of the inventory screen. Drag an item from the inventory onto a slot to equip it there; right-click, double-click or drag it out to take it off
 *  Your own look: a Widget Blueprint with this parent class. Put Item Slot widgets (ZItemSlotWidget) in it wherever you like, and set each one's Equipment Slot
 *  (the kind, e.g. Equipment.Slot.Ring, and which one from 0) in its Details. ArmorText is filled in too (optional).
 *  A Widget Blueprint without any widgets of its own gets the built-in look: every slot, in groups (weapons, armor, jewelry)
 */
UCLASS()
class UZEquipmentPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** The screen it belongs to: its slots select, describe and equip through it (the inventory screen calls this) */
	void SetScreen(UZInventoryScreenWidget* InScreen);

	/** Shows what's in every slot (happens by itself when the equipment changes) */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	virtual void Refresh();

	/** The widget showing a slot, or null */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UZItemSlotWidget* FindSlotWidget(const FZEquipmentSlot& EquipmentSlot) const;

	/** Highlights the slot holding this item (the screen's selection) */
	void SetSelectedItem(const UZItemInstance* Item);

	/** "Armor 25; Long gun: ID_Rifle, Head: (empty), ..." for logs and tests */
	FString GetDebugString() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	UFUNCTION()
	void HandleEquipmentChanged(UZEquipmentComponent* ChangedEquipment);

	/** One widget per slot in the built-in look (your own Widget Blueprint with UZItemSlotWidget as its parent class, or the built-in look) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UZItemSlotWidget> ItemSlotClass;

	/** "Armor 25: you take 80% damage" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArmorText;

private:

	/** Listens to the owning player's equipment and armor */
	void BindToPlayer();

	void UnbindFromPlayer();

	void HandleArmorChanged(const FOnAttributeChangeData& Data);

	void RefreshArmor();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZItemSlotWidget>> SlotWidgets;

	TWeakObjectPtr<UZEquipmentComponent> Equipment;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UZInventoryScreenWidget> Screen;
	FDelegateHandle ArmorChangedHandle;
};
