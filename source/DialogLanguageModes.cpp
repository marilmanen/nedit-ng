
#include "DialogLanguageModes.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "SignalBlocker.h"
#include "TextArea.h"
#include "highlightData.h"
#include "preferences.h"
#include "regularExp.h"
#include "smartIndent.h"
#include "tags.h"
#include "userCmds.h"
#include "search.h"
#include <QIntValidator>
#include <QMessageBox>
#include <QRegExp>
#include <QRegExpValidator>
#include <QString>
#include <QStringList>
#include <QtDebug>


//------------------------------------------------------------------------------
// Name: DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::DialogLanguageModes(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);

    for(const LanguageMode &lang : LanguageModes) {
        auto ptr  = new LanguageMode(lang);
		auto item = new QListWidgetItem(ptr->name);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}

	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	ui.editName->setValidator(new QRegExpValidator(QRegExp(QLatin1String("[\\sA-Za-z0-9_+$#-]+")), this));

	// 0-100
	ui.editTabSpacing->setValidator(new QIntValidator(0, 100, this));
	ui.editEmulatedTabSpacing->setValidator(new QIntValidator(-1, 100, this));
}

//------------------------------------------------------------------------------
// Name: ~DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::~DialogLanguageModes() noexcept {
	for(int i = 0; i < ui.listItems->count(); ++i) {
		delete itemFromIndex(i);
	}
}

//------------------------------------------------------------------------------
// Name: itemFromIndex
//------------------------------------------------------------------------------
LanguageMode *DialogLanguageModes::itemFromIndex(int i) const {
	if(i < ui.listItems->count()) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<LanguageMode *>(item->data(Qt::UserRole).toULongLong());
		return ptr;
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Name: on_listItems_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogLanguageModes::on_listItems_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		previous_ = nullptr;
		return;
	}

	QListWidgetItem *const current = selections[0];

	if(previous_ != nullptr && current != nullptr && current != previous_) {

		// we want to try to save it (but not apply it yet)
		// and then move on

		// NOTE(eteran): in the original nedit 5.6, this code is broken!
		//               it results in a crash when leaving invalid entries :-(
		//               we can do better of course
#if 0
        if(!checkCurrent(true)) {

			/* If there are problems, and the user didn't ask for the fields to be
			   read, give more warning */

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Discard Language Mode"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current language mode?"));

			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
			QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {

				// again to cause messagebox to pop up
#if 0
				checkCurrent(false);
#endif

				// reselect the old item
                no_signals(ui.listItems)->setCurrentItem(previous_);
				return;
			}

			// if we get here, we are ditching changes
		} else {
			if(!updateCurrentItem(previous_)) {
				return;
			}
		}
#endif
	}


	if(current) {

		const int i = ui.listItems->row(current);

		LanguageMode *language = itemFromIndex(i);

		QStringList extensions;
		for(QString &extension : language->extensions) {
			extensions.push_back(extension);
		}

		ui.editName      ->setText(language->name);
		ui.editExtensions->setText(extensions.join(QLatin1String(" ")));
		ui.editRegex     ->setText(language->recognitionExpr);
		ui.editCallTips  ->setText(language->defTipsFile);
		ui.editDelimiters->setText(language->delimiters);

		if(language->tabDist != -1) {
			ui.editTabSpacing->setText(QString::number(language->tabDist));
		} else {
			ui.editTabSpacing->setText(QString());
		}

		if(language->emTabDist != -1) {
			ui.editEmulatedTabSpacing->setText(QString::number(language->emTabDist));
		} else {
			ui.editEmulatedTabSpacing->setText(QString());
		}

		switch(language->indentStyle) {
        case IndentStyle::None:
			ui.radioIndentNone->setChecked(true);
			break;
        case IndentStyle::Auto:
			ui.radioIndentAuto->setChecked(true);
			break;
        case IndentStyle::Smart:
			ui.radioIndentSmart->setChecked(true);
			break;
		default:
			ui.radioIndentDefault->setChecked(true);
			break;
		}

		switch(language->wrapStyle) {
        case WrapStyle::None:
			ui.radioWrapNone->setChecked(true);
			break;
        case WrapStyle::Newline:
			ui.radioWrapAuto->setChecked(true);
			break;
        case WrapStyle::Continuous:
			ui.radioWrapContinuous->setChecked(true);
			break;
		default:
			ui.radioWrapDefault->setChecked(true);
			break;
		}

		if(i == 0) {
			ui.buttonUp    ->setEnabled(false);
			ui.buttonDown  ->setEnabled(ui.listItems->count() > 2);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(i == ui.listItems->count() - 1) {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(false);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(true);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		}
	}

	previous_ = current;
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonBox_accepted() {
    if (!updateLMList(Mode::Verbose)) {
		return;
	}

	accept();
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
        updateLMList(Mode::Verbose);
	}
}

/*
** Read the fields in the language modes dialog and create a LanguageMode data
** structure reflecting the current state of the selected language mode in the dialog.
** If any of the information is incorrect or missing, display a warning dialog and
** return nullptr.  Passing "silent" as True, suppresses the warning dialogs.
*/
std::unique_ptr<LanguageMode> DialogLanguageModes::readLMDialogFields(Mode mode) {

	/* Allocate a language mode structure to return, set unread fields to
	   empty so everything can be freed on errors by freeLanguageModeRec */
    auto lm = std::make_unique<LanguageMode>();

	// read the name field
	QString name = ui.editName->text().simplified();
	if (name.isEmpty()) {
        if (mode == Mode::Verbose) {
			QMessageBox::warning(this, tr("Language Mode Name"), tr("Please specify a name for the language mode"));
		}
		return nullptr;
	}

	lm->name = name;

	// read the extension list field
	QString extStr      = ui.editExtensions->text().simplified();
	QStringList extList = extStr.split(QLatin1Char(' '), QString::SkipEmptyParts);
	lm->extensions = extList;

	// read recognition expression
	QString recognitionExpr = ui.editRegex->text();
	if(!recognitionExpr.isEmpty()) {
		try {
            auto compiledRE = make_regex(recognitionExpr, REDFLT_STANDARD);
		} catch(const regex_error &e) {
            if (mode == Mode::Verbose) {
                QMessageBox::warning(this, tr("Regex"), tr("Recognition expression:\n%1").arg(QString::fromLatin1(e.what())));
			}
			return nullptr;
		}
	}
    lm->recognitionExpr = !recognitionExpr.isEmpty() ? recognitionExpr : QString();

	// Read the default calltips file for the language mode
	QString tipsFile = ui.editCallTips->text();
	if(!tipsFile.isEmpty()) {
		// Ensure that AddTagsFile will work
        if (!AddTagsFileEx(tipsFile, TagSearchMode::TIP)) {
            if (mode == Mode::Verbose) {
				QMessageBox::warning(this, tr("Error reading Calltips"), tr("Can't read default calltips file(s):\n  \"%1\"\n").arg(tipsFile));
			}
			return nullptr;
        } else if (!DeleteTagsFileEx(tipsFile, TagSearchMode::TIP, false)) {
            qCritical("NEdit: Internal error: Trouble deleting calltips file(s):\n  \"%s\"", qPrintable(tipsFile));
		}
	}
    lm->defTipsFile = !tipsFile.isEmpty() ? tipsFile : QString();

	// read tab spacing field
	QString tabsSpacing = ui.editTabSpacing->text();
	if(tabsSpacing.isEmpty()) {
		lm->tabDist = LanguageMode::DEFAULT_TAB_DIST;
	} else {
		bool ok;
		int tabsSpacingValue = tabsSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in tab spacing").arg(tabsSpacing));
			return nullptr;
		}

		lm->tabDist = tabsSpacingValue;

		if (lm->tabDist <= 0 || lm->tabDist > 100) {
            if (mode == Mode::Verbose) {
				QMessageBox::warning(this, tr("Invalid Tab Spacing"), tr("Invalid tab spacing: %1").arg(lm->tabDist));
			}
			return nullptr;
		}
	}

	// read emulated tab field
	QString emulatedTabSpacing = ui.editEmulatedTabSpacing->text();
	if(emulatedTabSpacing.isEmpty()) {
		lm->emTabDist = LanguageMode::DEFAULT_EM_TAB_DIST;
	} else {
		bool ok;
		int emulatedTabSpacingValue = emulatedTabSpacing.toInt(&ok);
		if (!ok) {
			QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in emulated tab spacing").arg(tabsSpacing));
			return nullptr;
		}

		lm->emTabDist = emulatedTabSpacingValue;

		if (lm->emTabDist < 0 || lm->emTabDist > 100) {
            if (mode == Mode::Verbose) {
				QMessageBox::warning(this, tr("Invalid Tab Spacing"), tr("Invalid emulated tab spacing: %1").arg(lm->emTabDist));
			}
			return nullptr;
		}
	}

	// read delimiters string
	QString delimiters = ui.editDelimiters->text();
	if(!delimiters.isEmpty()) {
		lm->delimiters = delimiters;
	}

	// read indent style
	if(ui.radioIndentNone->isChecked()) {
        lm->indentStyle = IndentStyle::None;
	} else if(ui.radioIndentAuto->isChecked()) {
        lm->indentStyle = IndentStyle::Auto;
	} else if(ui.radioIndentSmart->isChecked()) {
        lm->indentStyle = IndentStyle::Smart;
	} else if(ui.radioIndentDefault->isChecked()) {
        lm->indentStyle = IndentStyle::Default;
	}

	// read wrap style
	if(ui.radioWrapNone->isChecked()) {
        lm->wrapStyle = WrapStyle::None;
	} else if(ui.radioWrapAuto->isChecked()) {
        lm->wrapStyle = WrapStyle::Newline;
	} else if(ui.radioWrapContinuous->isChecked()) {
        lm->wrapStyle = WrapStyle::Continuous;
	} else if(ui.radioWrapDefault->isChecked()) {
        lm->wrapStyle = WrapStyle::Default;
	}

    return lm;
}



//------------------------------------------------------------------------------
// Name: updateLanguageList
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateLanguageList(Mode mode) {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listItems->row(selection);
	LanguageMode *oldLM = itemFromIndex(i);

    if(auto newLM = readLMDialogFields(mode)) {

		/* If there was a name change of a non-duplicate language mode, modify the
		   name to the weird format of: "old name:new name".  This signals that a
		   name change is necessary in lm dependent data such as highlight
		   patterns.  Duplicate language modes may be re-named at will, since no
		   data will be lost due to the name change. */
		if (oldLM != nullptr && oldLM->name != newLM->name) {
			int nCopies = 0;

			for (int i = 0; i < ui.listItems->count(); i++) {
				if (oldLM->name == itemFromIndex(i)->name) {
					nCopies++;
				}
			}

			if (nCopies <= 1) {
				int index = oldLM->name.indexOf(QLatin1Char(':'));
				int oldLen = (index == -1) ? oldLM->name.size() : index;

				auto tempName = tr("%1:%2").arg(oldLM->name.mid(0, oldLen), newLM->name);
				newLM->name = tempName;
			}
		}

		QListWidgetItem* item = ui.listItems->item(i);
        item->setText(newLM->name);
        item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(newLM.release()));
		delete oldLM;
		return true;
	}


	return false;
}

//------------------------------------------------------------------------------
// Name: updateLMList
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateLMList(Mode mode) {


	// Get the current contents of the dialog fields
    if(!updateLanguageList(mode)) {
		return false;
	}

    if(auto window = qobject_cast<MainWindow *>(parent())) {

        /* Fix up language mode indices in all open windows (which may change
           if the currently selected mode is deleted or has changed position),
           and update word delimiters */
        for(DocumentWidget *document: DocumentWidget::allDocuments()) {
            if (document->languageMode_ != PLAIN_LANGUAGE_MODE) {

                QString oldModeName = LanguageModes[document->languageMode_].name;
                document->languageMode_ = PLAIN_LANGUAGE_MODE;

                for (int i = 0; i < ui.listItems->count(); i++) {

                    if (oldModeName == itemFromIndex(i)->name) {
                        QString newDelimiters = itemFromIndex(i)->delimiters;

                        if(newDelimiters.isNull()) {
                            newDelimiters = GetPrefDelimiters();
                        }

                        for(TextArea *area : document->textPanes()) {
                            area->setWordDelimiters(newDelimiters);
                        }

                        // don't forget to adapt the LM stored within the user menu cache
                        window->updateLanguageModeSubmenu();

                        // update the language mode of this window (document)
                        document->languageMode_ = i;
                        break;
                    }
                }
            }
        }


        /* If there were any name changes, re-name dependent highlight patterns
           and smart-indent macros and fix up the weird rename-format names */
        // naming causes the name to be set to the format of "Old:New"
        // update names appropriately
        for (int i = 0; i < ui.listItems->count(); i++) {
            QStringList parts = itemFromIndex(i)->name.split(QLatin1Char(':'));

            if (parts.size() == 2) {
                QString oldName = parts[0];
                QString newName = parts[1];

                RenameHighlightPattern(oldName, newName);
                RenameSmartIndentMacros(oldName, newName);
                itemFromIndex(i)->name = newName;
            }
        }

        // Replace the old language mode list with the new one from the dialog
        LanguageModes.clear();

        for (int i = 0; i < ui.listItems->count(); i++) {
            LanguageModes.push_back(*itemFromIndex(i));
        }

        /* Update user menu info to update language mode dependencies of
           user menu items */
        UpdateUserMenuInfo();

        /* Update the menus in the window menu bars and load any needed
            calltips files */
        for(MainWindow *win : MainWindow::allWindows()) {
            win->updateLanguageModeSubmenu();

        }

        /* Update the menus in the window menu bars and load any needed
            calltips files */
        for(DocumentWidget *document: DocumentWidget::allDocuments()) {

            if (document->languageMode_ != PLAIN_LANGUAGE_MODE && !LanguageModes[document->languageMode_].defTipsFile.isNull()) {
                AddTagsFileEx(LanguageModes[document->languageMode_].defTipsFile, TagSearchMode::TIP);
            }
        }

        // If a syntax highlighting dialog is up, update its menu
        UpdateLanguageModeMenu();

		// The same for the smart indent macro dialog
        UpdateLangModeMenuSmartIndent();

        // Note that preferences have been changed
        MarkPrefsChanged();
    }

	return true;
}

//------------------------------------------------------------------------------
// Name: on_buttonNew_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonNew_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	auto ptr  = new LanguageMode;
	ptr->name = tr("New Item");

	auto item = new QListWidgetItem(ptr->name);
	item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
	ui.listItems->addItem(item);
	ui.listItems->setCurrentItem(item);
}

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonCopy_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<LanguageMode *>(selection->data(Qt::UserRole).toULongLong());
	auto newPtr = new LanguageMode(*ptr);
	auto newItem = new QListWidgetItem(newPtr->name);
	newItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(newPtr));

	const int i = ui.listItems->row(selection);
	ui.listItems->insertItem(i + 1, newItem);
	ui.listItems->setCurrentItem(newItem);
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonUp_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listItems->row(selection);
	if(i != 0) {
		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i - 1, item);
		ui.listItems->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonDown_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listItems->row(selection);
	if(i != ui.listItems->count() - 1) {

		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i + 1, item);
		ui.listItems->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonDelete_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonDelete_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return; // false
	}

	QListWidgetItem *const selection = selections[0];

	const int itemIndex = ui.listItems->row(selection);

	// Allow duplicate names to be deleted regardless of dependencies
	for (int i = 0; i < ui.listItems->count(); i++) {
		if (i != itemIndex && itemFromIndex(i)->name == itemFromIndex(itemIndex)->name) {


			previous_ = nullptr;

			delete itemFromIndex(itemIndex);
			delete selection;
			// force an update of the display
			on_listItems_itemSelectionChanged();
			return; // True;
		}
	}

	// don't allow deletion if data will be lost
	if (LMHasHighlightPatterns(itemFromIndex(itemIndex)->name)) {
		QMessageBox::warning(this, tr("Patterns exist"), tr("This language mode has syntax highlighting patterns defined.  Please delete the patterns first, in Preferences -> Default Settings -> Syntax Highlighting, before proceeding here."));
		return; // False;
	}

	// don't allow deletion if data will be lost
    if (LMHasSmartIndentMacros(itemFromIndex(itemIndex)->name)) {
		QMessageBox::warning(this, tr("Smart Indent Macros exist"), tr("This language mode has smart indent macros defined.  Please delete the macros first, in Preferences -> Default Settings -> Auto Indent -> Program Smart Indent, before proceeding here."));
		return; // False;
	}

	previous_ = nullptr;

	delete itemFromIndex(itemIndex);
	delete selection;
	on_listItems_itemSelectionChanged();
	return;// True;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateCurrentItem(QListWidgetItem *item) {
	// Get the current contents of the "patterns" dialog fields
    auto ptr = readLMDialogFields(Mode::Verbose);
	if(!ptr) {
		return false;
	}

	// delete the current pattern in this slot
	auto old = reinterpret_cast<LanguageMode *>(item->data(Qt::UserRole).toULongLong());
	delete old;

    item->setText(ptr->name);
    item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr.release()));
	return true;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool DialogLanguageModes::updateCurrentItem() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];
	return updateCurrentItem(selection);
}
