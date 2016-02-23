// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerLabelListRow.h"


#define LOCTEXT_NAMESPACE "SSequencerLabelBrowser"


/* SSequencerLabelBrowser structors
 *****************************************************************************/

SSequencerLabelBrowser::~SSequencerLabelBrowser()
{
}


/* SSequencerLabelBrowser interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSequencerLabelBrowser::Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;
	Sequencer = InSequencer;

	ChildSlot
	[
		SAssignNew(LabelTreeView, STreeView<TSharedPtr<FSequencerLabelTreeNode>>)
			.ItemHeight(20.0f)
			.OnContextMenuOpening(this, &SSequencerLabelBrowser::HandleLabelTreeViewContextMenuOpening)
			.OnGenerateRow(this, &SSequencerLabelBrowser::HandleLabelTreeViewGenerateRow)
			.OnGetChildren(this, &SSequencerLabelBrowser::HandleLabelTreeViewGetChildren)
			.OnSelectionChanged(this, &SSequencerLabelBrowser::HandleLabelTreeViewSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
			.TreeItemsSource(&LabelList)
	];

	Sequencer.Pin()->GetLabelManager().OnLabelsChanged().AddSP(this, &SSequencerLabelBrowser::HandleLabelManagerLabelsChanged);

	ReloadLabelList(true);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void SSequencerLabelBrowser::SetSelectedLabel(const FString& Label)
{
	if (!Label.IsEmpty())
	{
		for (auto& Node : LabelList)
		{
			if (Node->Label == Label)
			{
				LabelTreeView->SetSelection(Node);

				return;
			}
		}
	}

	LabelTreeView->ClearSelection();
}


/* SSequencerLabelBrowser implementation
 *****************************************************************************/

void SSequencerLabelBrowser::ReloadLabelList(bool FullyReload)
{
	LabelList.Reset();
	LabelList.Add(MakeShareable(new FSequencerLabelTreeNode(FString(), FText::GetEmpty())));

	TArray<FString> AllLabels;
	
	if (Sequencer.IsValid() && Sequencer.Pin()->GetLabelManager().GetAllLabels(AllLabels) > 0)
	{
		for (const auto& Label : AllLabels)
		{
			// create new leaf node
			TArray<FString> Strings;
			Label.ParseIntoArray(Strings, TEXT("."), true);

			TSharedRef<FSequencerLabelTreeNode> NewNode = MakeShareable(
				new FSequencerLabelTreeNode(Label, FText::FromString(Strings.Last())));

			// insert node into tree
			TArray<TSharedPtr<FSequencerLabelTreeNode>>* ParentNodes = &LabelList;
			int32 Index = 0;

			while (Index < Strings.Num() - 1)
			{
				TSharedPtr<FSequencerLabelTreeNode> Parent;

				for (const auto& Node : *ParentNodes)
				{
					if (Node->Label == Strings[Index])
					{
						Parent = Node;
						break;
					}
				}

				// create interior node if needed
				if (!Parent.IsValid())
				{
					FString ParentLabel = Strings[0];

					for (int32 SubIndex = 1; SubIndex <= Index; ++SubIndex)
					{
						ParentLabel += TEXT(".") + Strings[SubIndex];
					}

					Parent = MakeShareable(new FSequencerLabelTreeNode(ParentLabel, FText::FromString(Strings[Index])));
					ParentNodes->Add(Parent);
				}

				ParentNodes = &Parent->Children;
				++Index;
			}

			// insert node into tree
			ParentNodes->Add(NewNode);
		}
	}

	LabelTreeView->RequestTreeRefresh();
}


/* SSequencerLabelBrowser callbacks
 *****************************************************************************/

void SSequencerLabelBrowser::HandleLabelListRowLabelRenamed(TSharedPtr<FSequencerLabelTreeNode> Node, const FString& NewLabel)
{
	if (Sequencer.IsValid() && Sequencer.Pin()->GetLabelManager().RenameLabel(Node->Label, NewLabel))
	{
		ReloadLabelList(true);
	}
}


void SSequencerLabelBrowser::HandleLabelManagerLabelsChanged()
{
	ReloadLabelList(true);
}


TSharedPtr<SWidget> SSequencerLabelBrowser::HandleLabelTreeViewContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true /*bInShouldCloseWindowAfterMenuSelection*/, nullptr);
	{
		MenuBuilder.BeginSection("Edit", LOCTEXT("EditContextMenuSectionName", "Edit"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RemoveLabelMenuEntryLabel", "Remove"),
				LOCTEXT("RemoveLabelMenuEntryTip", "Remove this label from this list and all tracks"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRemoveLabelMenuEntryExecute),
					FCanExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRemoveLabelMenuEntryCanExecute)
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenameLabelMenuEntryLabel", "Rename"),
				LOCTEXT("RenameLabelMenuEntryTip", "Change the name of this label"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRenameLabelMenuEntryExecute),
					FCanExecuteAction::CreateSP(this, &SSequencerLabelBrowser::HandleRenameLabelMenuEntryCanExecute)
				)
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}


 TSharedRef<ITableRow> SSequencerLabelBrowser::HandleLabelTreeViewGenerateRow(TSharedPtr<FSequencerLabelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSequencerLabelListRow, OwnerTable)
		.Node(Item)
		.OnLabelRenamed(this, &SSequencerLabelBrowser::HandleLabelListRowLabelRenamed);
}


void SSequencerLabelBrowser::HandleLabelTreeViewGetChildren(TSharedPtr<FSequencerLabelTreeNode> Item, TArray<TSharedPtr<FSequencerLabelTreeNode>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}


void SSequencerLabelBrowser::HandleLabelTreeViewSelectionChanged(TSharedPtr<FSequencerLabelTreeNode> InItem, ESelectInfo::Type SelectInfo)
{
	OnSelectionChanged.ExecuteIfBound(
		InItem.IsValid()
			? InItem->Label
			: FString(),
		SelectInfo
	);
}


void SSequencerLabelBrowser::HandleRemoveLabelMenuEntryExecute()
{
	TArray<TSharedPtr<FSequencerLabelTreeNode>> SelectedItems;

	if (Sequencer.IsValid() && LabelTreeView->GetSelectedItems(SelectedItems) > 0)
	{
		Sequencer.Pin()->GetLabelManager().RemoveObjectLabel(FGuid(), SelectedItems[0]->Label);
	}
}


bool SSequencerLabelBrowser::HandleRemoveLabelMenuEntryCanExecute() const
{
	TArray<TSharedPtr<FSequencerLabelTreeNode>> SelectedItems;

	if (LabelTreeView->GetSelectedItems(SelectedItems) == 0)
	{
		return false;
	}

	return !SelectedItems[0]->Label.IsEmpty();
}


void SSequencerLabelBrowser::HandleRenameLabelMenuEntryExecute()
{
	TArray<TSharedPtr<FSequencerLabelTreeNode>> SelectedItems;

	if (LabelTreeView->GetSelectedItems(SelectedItems) > 0)
	{
		auto ListRow = StaticCastSharedPtr<SSequencerLabelListRow>(LabelTreeView->WidgetFromItem(SelectedItems[0]));

		if (ListRow.IsValid())
		{
			ListRow->EnterRenameMode();
		}
	}
}


bool SSequencerLabelBrowser::HandleRenameLabelMenuEntryCanExecute() const
{
	TArray<TSharedPtr<FSequencerLabelTreeNode>> SelectedItems;
	return (LabelTreeView->GetSelectedItems(SelectedItems) > 0);
}


#undef LOCTEXT_NAMESPACE
