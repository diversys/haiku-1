/*
 * JobSetupDlg.cpp
 * Copyright 1999-2000 Y.Takagi. All Rights Reserved.
 */

#ifndef __JOBSETUPDLG_H
#define __JOBSETUPDLG_H

#include <View.h>
#include "DialogWindow.h"

#include "JobData.h"
#include "Halftone.h"
#include "JSDSlider.h"
#include "PrinterCap.h"

class BTextControl;
class BTextView;
class BRadioButton;
class BCheckBox;
class BPopUpMenu;
class JobData;
class PrinterData;
class PrinterCap;
class HalftoneView;
class PagesView;

class JobSetupView : public BView {
public:
	JobSetupView(JobData* job_data, PrinterData* printer_data,
		const PrinterCap* printer_cap);
	virtual void AttachedToWindow();
	virtual void MessageReceived(BMessage* msg);
	bool UpdateJobData(bool showPreview);

private:
	void UpdateButtonEnabledState();
	void FillCapabilityMenu(BPopUpMenu* menu, uint32 message,
		PrinterCap::CapID category, int id);
	void FillCapabilityMenu(BPopUpMenu* menu, uint32 message,
		const BaseCap** capabilities, int count, int id);
	int	GetID(const BaseCap** capabilities, int count, const char* label,
		int defaultValue);
	BRadioButton* CreatePageSelectionItem(const char* name, const char* label,
		JobData::PageSelection pageSelection);
	void AllowOnlyDigits(BTextView* textView, int maxDigits);
	JobData::Color Color();
	Halftone::DitherType DitherType();
	float Gamma();
	float InkDensity();
	JobData::PaperSource PaperSource();

	BTextControl     *fCopies;
	BTextControl     *fFromPage;
	BTextControl     *fToPage;
	JobData          *fJobData;
	PrinterData      *fPrinterData;
	const PrinterCap *fPrinterCap;
	BPopUpMenu       *fColorType;
	BPopUpMenu       *fDitherType;
	JSDSlider        *fGamma;
	JSDSlider        *fInkDensity;
	HalftoneView     *fHalftone;
	BRadioButton     *fAll;
	BCheckBox        *fCollate;
	BCheckBox        *fReverse;
	PagesView        *fPages;
	BPopUpMenu       *fPaperFeed;
	BCheckBox        *fDuplex;
	BPopUpMenu       *fNup;
	BRadioButton     *fAllPages;
	BRadioButton     *fOddNumberedPages;
	BRadioButton     *fEvenNumberedPages;	
};

class JobSetupDlg : public DialogWindow {
public:
	JobSetupDlg(JobData *job_data, PrinterData *printer_data, const PrinterCap *printer_cap);
	virtual	void MessageReceived(BMessage *message);

private:
	JobSetupView *fJobSetup;
};

#endif	/* __JOBSETUPDLG_H */
