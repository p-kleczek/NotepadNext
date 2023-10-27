/*
 * This file is part of Notepad Next.
 * Copyright 2019 Justin Dailey
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "FindReplaceDialog.h"
#include "ui_FindReplaceDialog.h"

#include <QSettings>
#include <QStatusBar>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFileDialog>

#include "ScintillaNext.h"
#include "MainWindow.h"


static void convertToExtended(QString &str)
{
    str.replace("\\r", "\r");
    str.replace("\\n", "\n");
    str.replace("\\t", "\t");
    str.replace("\\0", "\0");
    str.replace("\\\\", "\\");
    // TODO: more
}

FindReplaceDialog::FindReplaceDialog(ISearchResultsHandler *searchResults, MainWindow *window) :
    QDialog(window, Qt::Dialog),
    ui(new Ui::FindReplaceDialog),
    searchResultsHandler(searchResults),
    finder(new Finder(window->currentEditor()))
{
    qInfo(Q_FUNC_INFO);

    // Turn off the help button on the dialog
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    ui->setupUi(this);

    // Get the current editor, and keep up the reference
    setEditor(window->currentEditor());
    connect(window, &MainWindow::editorActivated, this, &FindReplaceDialog::setEditor);

    tabBar = new QTabBar();
    tabBar->addTab(tr("Find"));
    tabBar->addTab(tr("Replace"));
    tabBar->addTab(tr("Find in Files"));
    tabBar->setExpanding(false);
    qobject_cast<QVBoxLayout *>(layout())->insertWidget(0, tabBar);
    connect(tabBar, &QTabBar::currentChanged, this, &FindReplaceDialog::changeTab);

    statusBar = new QStatusBar();
    statusBar->setSizeGripEnabled(false); // the dialog has one already
    qobject_cast<QVBoxLayout *>(layout())->insertWidget(-1, statusBar);

    // Disable auto completion
    ui->comboFind->setCompleter(nullptr);
    ui->comboReplace->setCompleter(nullptr);
    ui->comboFilters->setCompleter(nullptr);
    ui->comboDirectory->setCompleter(nullptr);

    // If the selection changes highlight the text
    connect(ui->comboFind, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), ui->comboFind->lineEdit(), &QLineEdit::selectAll);
    connect(ui->comboReplace, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), ui->comboReplace->lineEdit(), &QLineEdit::selectAll);
    connect(ui->comboFilters, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), ui->comboFilters->lineEdit(), &QLineEdit::selectAll);
    connect(ui->comboDirectory, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), ui->comboDirectory->lineEdit(), &QLineEdit::selectAll);

    // Force focus on the find text box
    connect(this, &FindReplaceDialog::windowActivated, [=]() {
        ui->comboFind->setFocus();
        ui->comboFind->lineEdit()->selectAll();
    });

    connect(this, &QDialog::rejected, [=]() {
        statusBar->clearMessage();
        savePosition();
    });

    connect(ui->radioRegexSearch, &QRadioButton::toggled, this, [=](bool checked) {
        ui->checkBoxBackwardsDirection->setDisabled(checked);
        ui->checkBoxMatchWholeWord->setDisabled(checked);
        ui->checkBoxRegexMatchesNewline->setEnabled(checked);
    });

    connect(ui->radioOnLosingFocus, &QRadioButton::toggled, this, &FindReplaceDialog::adjustOpacityWhenLosingFocus);
    connect(ui->radioAlways, &QRadioButton::toggled, this, &FindReplaceDialog::adjustOpacityAlways);
    connect(ui->transparency, &QGroupBox::toggled, this, &FindReplaceDialog::transparencyToggled);

    connect(ui->buttonFind, &QPushButton::clicked, this, &FindReplaceDialog::find);
    connect(ui->buttonCount, &QPushButton::clicked, this, &FindReplaceDialog::count);
    connect(ui->buttonFindAllInCurrent, &QPushButton::clicked, this, [=]() {
        prepareToPerformSearch();

        searchResultsHandler->newSearch(findString());

        findAllInCurrentDocument();

        searchResultsHandler->completeSearch();

        close();
    });
    connect(ui->buttonFindAllInDocuments, &QPushButton::clicked, this, [=]() {
        prepareToPerformSearch();

        searchResultsHandler->newSearch(findString());

        findAllInDocuments();

        searchResultsHandler->completeSearch();

        close();
    });
    connect(ui->buttonReplace, &QPushButton::clicked, this, &FindReplaceDialog::replace);
    connect(ui->buttonReplaceAll, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
    connect(ui->buttonReplaceAllInDocuments, &QPushButton::clicked, this, [=]() {
        prepareToPerformSearch(true);

        QString replaceText = replaceString();

        if (ui->radioExtendedSearch->isChecked()) {
            convertToExtended(replaceText);
        }

        int count = 0;
        ScintillaNext *current_editor = editor;
        MainWindow *window = qobject_cast<MainWindow *>(parent());

        for(ScintillaNext *editor : window->editors()) {
            setEditor(editor);
            count += finder->replaceAll(replaceText);
        }

        setEditor(current_editor);

        showMessage(tr("Replaced %Ln matches", "", count), "green");
    });
    connect(ui->buttonFindAll, &QPushButton::clicked, this, [=]() {
        prepareToPerformSearch();
        searchResultsHandler->newSearch(findString());
        findInFiles();
        searchResultsHandler->completeSearch();
        close();
    });
    
    // FIXME: Add button "Replace in Files"
    connect(ui->buttonFindInFilesBrowse, &QPushButton::clicked, this, &FindReplaceDialog::selectFolderDialog);
    // FIXME: Add UI elements: "Follow current doc"

    connect(ui->buttonClose, &QPushButton::clicked, this, &FindReplaceDialog::close);

    loadSettings();

    connect(qApp, &QApplication::aboutToQuit, this, &FindReplaceDialog::saveSettings);

    changeTab(tabBar->currentIndex());
}

FindReplaceDialog::~FindReplaceDialog()
{
    delete ui;
    delete finder;
}

void FindReplaceDialog::setFindString(const QString &string)
{
    ui->comboFind->setCurrentText(string);
    ui->comboFind->lineEdit()->selectAll();
}

void FindReplaceDialog::setTab(int tab)
{
    tabBar->setCurrentIndex(tab);
}

bool FindReplaceDialog::event(QEvent *event)
{
    if (event->type() == QEvent::WindowActivate) {
        emit windowActivated();
    }
    else if (event->type() == QEvent::WindowDeactivate) {
        emit windowDeactivated();
    }

    return QDialog::event(event);
}

void FindReplaceDialog::showEvent(QShowEvent *event)
{
    qInfo(Q_FUNC_INFO);

    if (!isFirstTime)
        restorePosition();

    isFirstTime = false;

    QDialog::showEvent(event);
}

static void updateComboList(QComboBox *comboBox, const QString &text)
{
    // Block the signals while it is manipulated
    const QSignalBlocker blocker(comboBox);

    // Remove it if it is in the list, add it to beginning, and select it
    comboBox->removeItem(comboBox->findText(text));
    comboBox->insertItem(0, text);
    comboBox->setCurrentIndex(0);
}

void FindReplaceDialog::updateFindList(const QString &text)
{
    if (!text.isEmpty())
        updateComboList(ui->comboFind, text);
}

void FindReplaceDialog::updateReplaceList(const QString &text)
{
    updateComboList(ui->comboReplace, text);
}

void FindReplaceDialog::updateFiltersList(const QString &text)
{
    updateComboList(ui->comboFilters, text);
}

void FindReplaceDialog::updateDirectoryList(const QString &text)
{
    updateComboList(ui->comboDirectory, text);
}


void FindReplaceDialog::find()
{
    qInfo(Q_FUNC_INFO);

    prepareToPerformSearch();

    Sci_CharacterRange range = finder->findNext();

    if (ScintillaNext::isRangeValid(range)) {
        if (finder->didLatestSearchWrapAround()) {
            showMessage(tr("The end of the document has been reached. Found 1st occurrence from the top."), "green");
        }

        // TODO: Handle zero length matches better
        if (range.cpMin == range.cpMax) {
            qWarning() << "0 length match at" << range.cpMin;
        }

        editor->goToRange(range);
    }
    else {
        showMessage(tr("No matches found."), "red");
    }
}

void FindReplaceDialog::findAllInCurrentDocument()
{
    qInfo(Q_FUNC_INFO);

    bool firstMatch = true;

    QString text = findString();

    finder->setSearchText(text);
    finder->forEachMatch([&](int start, int end){
        // Only add the file entry if there was a valid search result
        if (firstMatch) {
            searchResultsHandler->newFileEntry(editor);
            firstMatch = false;
        }

        const int line = editor->lineFromPosition(start);
        const int lineStartPosition = editor->positionFromLine(line);
        const int lineEndPosition = editor->lineEndPosition(line);
        const int startPositionFromBeginning = start - lineStartPosition;
        const int endPositionFromBeginning = end - lineStartPosition;
        QString lineText = editor->get_text_range(lineStartPosition, lineEndPosition);

        searchResultsHandler->newResultsEntry(lineText, line, startPositionFromBeginning, endPositionFromBeginning);

        return end;
    });
}

void FindReplaceDialog::findAllInDocuments()
{
    qInfo(Q_FUNC_INFO);

    ScintillaNext *current_editor = editor;
    MainWindow *window = qobject_cast<MainWindow *>(parent());

    for(ScintillaNext *editor : window->editors()) {
        setEditor(editor);
        findAllInCurrentDocument();
    }

    setEditor(current_editor);
}

void FindReplaceDialog::replace()
{
    qInfo(Q_FUNC_INFO);

    prepareToPerformSearch();

    QString replaceText = replaceString();

    if (ui->radioExtendedSearch->isChecked()) {
        convertToExtended(replaceText);
    }

    Sci_CharacterRange range = finder->replaceSelectionIfMatch(replaceText);

    if (ScintillaNext::isRangeValid(range)) {
        showMessage(tr("1 occurrence was replaced"), "blue");
    }

    Sci_CharacterRange next_match = finder->findNext();

    if (ScintillaNext::isRangeValid(next_match)) {
        editor->goToRange(next_match);
    }
    else {
        showMessage(tr("No more occurrences were found"), "red");
        ui->comboFind->setFocus();
        ui->comboFind->lineEdit()->selectAll();
    }
}

void FindReplaceDialog::replaceAll()
{
    qInfo(Q_FUNC_INFO);

    prepareToPerformSearch(true);

    QString replaceText = replaceString();

    if (ui->radioExtendedSearch->isChecked()) {
        convertToExtended(replaceText);
    }

    int count = finder->replaceAll(replaceText);
    showMessage(tr("Replaced %Ln matches", "", count), "green");
}

void FindReplaceDialog::count()
{
    qInfo(Q_FUNC_INFO);

    prepareToPerformSearch();

    int total = finder->count();

    showMessage(tr("Found %Ln matches", "", total), "green");
}

void FindReplaceDialog::setEditor(ScintillaNext *editor)
{
    this->editor = editor;

    finder->setEditor(editor);
}

void FindReplaceDialog::performLastSearch()
{
    editor->goToRange(finder->findNext());
}

void FindReplaceDialog::adjustOpacity(int value)
{
    qInfo(Q_FUNC_INFO);

    setWindowOpacity(value * .01);
}

void FindReplaceDialog::transparencyToggled(bool on)
{
    qInfo(Q_FUNC_INFO);

    if (on) {
        if (ui->radioOnLosingFocus->isChecked()) {
           adjustOpacityWhenLosingFocus(true);
           adjustOpacityAlways(false);
        }
        else {
            adjustOpacityWhenLosingFocus(false);
            adjustOpacityAlways(true);
        }
    }
    else {
        adjustOpacityWhenLosingFocus(false);
        adjustOpacityAlways(false);
        adjustOpacity(100);
    }
}

void FindReplaceDialog::adjustOpacityWhenLosingFocus(bool checked)
{
    qInfo(Q_FUNC_INFO);

    if (checked) {
        connect(this, &FindReplaceDialog::windowActivated, [=]() {
            this->adjustOpacity(100);
        });
        connect(this, &FindReplaceDialog::windowDeactivated, [=]() {
            this->adjustOpacity(ui->horizontalSlider->value());
        });
        adjustOpacity(100);
    }
    else {
        disconnect(this, &FindReplaceDialog::windowActivated, nullptr, nullptr);
        disconnect(this, &FindReplaceDialog::windowDeactivated, nullptr, nullptr);
    }
}

void FindReplaceDialog::adjustOpacityAlways(bool checked)
{
    qInfo(Q_FUNC_INFO);

    if (checked) {
        connect(ui->horizontalSlider, &QSlider::valueChanged, this, &FindReplaceDialog::adjustOpacity);
        adjustOpacity(ui->horizontalSlider->value());
    }
    else {
        disconnect(ui->horizontalSlider, &QSlider::valueChanged, this, &FindReplaceDialog::adjustOpacity);
    }
}

void FindReplaceDialog::changeTab(int index)
{
    if (index == FIND_TAB) {
        ui->labelReplaceWith->setMaximumHeight(0);
        ui->comboReplace->setMaximumHeight(0);
        // The combo box isn't actually "hidden", so adjust the focus policy so it does not get tabbed to
        ui->comboReplace->setFocusPolicy(Qt::NoFocus);

        ui->labelFilter->setMaximumHeight(0);
        ui->comboFilters->setMaximumHeight(0);
        ui->comboFilters->setFocusPolicy(Qt::NoFocus);

        ui->labelDirectory->setMaximumHeight(0);
        ui->comboDirectory->setMaximumHeight(0);
        ui->comboDirectory->setFocusPolicy(Qt::NoFocus);
        ui->buttonFindInFilesBrowse->setMaximumHeight(0);

        ui->buttonReplace->hide();
        ui->buttonReplaceAll->hide();
        ui->buttonReplaceAllInDocuments->hide();

        ui->buttonFind->show();
        ui->buttonCount->show();
        ui->buttonFindAllInCurrent->show();
        ui->buttonFindAllInDocuments->show();

        ui->buttonFindAll->hide();
        ui->buttonReplaceInFiles->hide();

        ui->checkBoxBackwardsDirection->show();
        ui->checkBoxWrapAround->show();

        ui->checkBoxInAllSubfolders->hide();
        ui->checkBoxInHiddenFolders->hide();
    }
    else if (index == REPLACE_TAB) {
        ui->labelReplaceWith->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboReplace->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboReplace->setFocusPolicy(Qt::StrongFocus); // Reset its focus policy

        ui->buttonFind->show();
        ui->buttonReplace->show();
        ui->buttonReplaceAll->show();
        ui->buttonReplaceAllInDocuments->show();

        ui->labelFilter->setMaximumHeight(0);
        ui->comboFilters->setMaximumHeight(0);
        ui->comboFilters->setFocusPolicy(Qt::NoFocus);

        ui->labelDirectory->setMaximumHeight(0);
        ui->comboDirectory->setMaximumHeight(0);
        ui->comboDirectory->setFocusPolicy(Qt::NoFocus);
        ui->buttonFindInFilesBrowse->setMaximumHeight(0);

        ui->buttonCount->hide();
        ui->buttonFindAllInCurrent->hide();
        ui->buttonFindAllInDocuments->hide();

        ui->buttonFindAll->hide();
        ui->buttonReplaceInFiles->hide();

        ui->checkBoxBackwardsDirection->show();
        ui->checkBoxWrapAround->show();

        ui->checkBoxInAllSubfolders->hide();
        ui->checkBoxInHiddenFolders->hide();
    }

    if (index == FIND_IN_FILES_TAB) {
        ui->labelReplaceWith->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboReplace->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboReplace->setFocusPolicy(Qt::StrongFocus); // Reset its focus policy

        ui->buttonFindAll->show();
        ui->buttonReplaceInFiles->show();

        ui->labelFilter->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboFilters->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboFilters->setFocusPolicy(Qt::StrongFocus);

        ui->labelDirectory->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboDirectory->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->comboDirectory->setFocusPolicy(Qt::StrongFocus);
        ui->buttonFindInFilesBrowse->setMaximumHeight(QWIDGETSIZE_MAX);

        ui->buttonFind->hide();
        ui->buttonCount->hide();
        ui->buttonFindAllInCurrent->hide();
        ui->buttonFindAllInDocuments->hide();
        ui->buttonReplace->hide();
        ui->buttonReplaceAll->hide();
        ui->buttonReplaceAllInDocuments->hide();

        ui->checkBoxBackwardsDirection->hide();
        ui->checkBoxWrapAround->hide();

        ui->checkBoxInAllSubfolders->show();
        ui->checkBoxInHiddenFolders->show();
    }

    ui->comboFind->setFocus();
    ui->comboFind->lineEdit()->selectAll();
}

QString FindReplaceDialog::findString()
{
    return ui->comboFind->currentText();
}

QString FindReplaceDialog::replaceString()
{
    return ui->comboReplace->currentText();
}

QString FindReplaceDialog::filterString()
{
    return ui->comboFilters->currentText();
}

QString FindReplaceDialog::directoryString()
{
    return ui->comboDirectory->currentText();
}


void FindReplaceDialog::setSearchResultsHandler(ISearchResultsHandler *searchResults)
{
    this->searchResultsHandler = searchResults;
}

void FindReplaceDialog::prepareToPerformSearch(bool replace)
{
    qInfo(Q_FUNC_INFO);

    QString findText = findString();

    updateFindList(findText);
    if (replace) {
        QString replaceText = replaceString();
        updateReplaceList(replaceText);
    }

    updateFiltersList(filterString());
    updateDirectoryList(directoryString());

    statusBar->clearMessage();

    if (ui->radioExtendedSearch->isChecked()) {
        convertToExtended(findText);
        //convertToExtended(replaceText);
    }

    finder->setWrap(ui->checkBoxWrapAround->isChecked());
    finder->setSearchFlags(computeSearchFlags());
    finder->setSearchText(findText);
}

void FindReplaceDialog::loadSettings()
{
    qInfo(Q_FUNC_INFO);

    QSettings settings;

    settings.beginGroup("FindReplaceDialog");

    ui->comboFind->addItems(settings.value("RecentSearchList").toStringList());
    ui->comboReplace->addItems(settings.value("RecentReplaceList").toStringList());
    ui->comboFilters->addItems(settings.value("RecentFiltersList").toStringList());
    ui->comboDirectory->addItems(settings.value("RecentDirectoriesList").toStringList());

    ui->checkBoxBackwardsDirection->setChecked(settings.value("Backwards").toBool());
    ui->checkBoxMatchWholeWord->setChecked(settings.value("WholeWord").toBool());
    ui->checkBoxMatchCase->setChecked(settings.value("MatchCase").toBool());
    ui->checkBoxWrapAround->setChecked(settings.value("WrapAround", true).toBool());

    if (settings.contains("SearchMode")) {
        const QString searchMode = settings.value("SearchMode").toString();
        if (searchMode == "normal")
            ui->radioNormalSearch->setChecked(true);
        else if (searchMode == "extended")
            ui->radioExtendedSearch->setChecked(true);
        else
            ui->radioRegexSearch->setChecked(true);
    }
    ui->checkBoxRegexMatchesNewline->setChecked(settings.value("DotMatchesNewline").toBool());

    ui->transparency->setChecked(settings.value("TransparencyUsed").toBool());
    if (ui->transparency->isChecked()) {
        ui->horizontalSlider->setValue(settings.value("Transparency", 70).toInt());

        if (settings.value("TransparencyMode").toString() == "focus") {
            ui->radioOnLosingFocus->setChecked(true);
        }
        else {
            ui->radioAlways->setChecked(true);
        }
    }

    ui->checkBoxInAllSubfolders->setChecked(settings.value("InAllSubfolders", true).toBool());
    ui->checkBoxInHiddenFolders->setChecked(settings.value("InHiddenFolders", true).toBool());

    settings.endGroup();
}

void FindReplaceDialog::saveSettings()
{
    qInfo(Q_FUNC_INFO);

    QSettings settings;

    settings.beginGroup("FindReplaceDialog");
    settings.remove(""); // clear out any previous keys

    QStringList recentSearches;
    for (int i = 0; i < ui->comboFind->count(); ++i) {
        recentSearches << ui->comboFind->itemText(i);
    }
    settings.setValue("RecentSearchList", recentSearches);

    recentSearches.clear();
    for (int i = 0; i < ui->comboReplace->count(); ++i) {
        recentSearches << ui->comboReplace->itemText(i);
    }
    settings.setValue("RecentReplaceList", recentSearches);

    QStringList recentFilters;
    for (int i = 0; i < ui->comboFilters->count(); ++i) {
        recentFilters << ui->comboFilters->itemText(i);
    }
    settings.setValue("RecentFiltersList", recentFilters);

    QStringList recentDirectories;
    for (int i = 0; i < ui->comboDirectory->count(); ++i) {
        recentDirectories << ui->comboDirectory->itemText(i);
    }
    settings.setValue("RecentDirectoriesList", recentDirectories);

    settings.setValue("Backwards", ui->checkBoxBackwardsDirection->isChecked());
    settings.setValue("WholeWord", ui->checkBoxMatchWholeWord->isChecked());
    settings.setValue("MatchCase", ui->checkBoxMatchCase->isChecked());
    settings.setValue("WrapAround", ui->checkBoxWrapAround->isChecked());

    if (ui->radioNormalSearch->isChecked())
        settings.setValue("SearchMode", "normal");
    else if (ui->radioExtendedSearch->isChecked())
        settings.setValue("SearchMode", "extended");
    else if (ui->radioRegexSearch->isChecked())
        settings.setValue("SearchMode", "regex");
    settings.setValue("DotMatchesNewline", ui->checkBoxRegexMatchesNewline->isChecked());

    settings.setValue("TransparencyUsed", ui->transparency->isChecked());
    if (ui->transparency->isChecked()) {
        settings.setValue("Transparency", ui->horizontalSlider->value());
        settings.setValue("TransparencyMode", ui->radioOnLosingFocus->isChecked() ? "focus" : "always");
    }

    settings.setValue("InAllSubfolders", ui->checkBoxInAllSubfolders->isChecked());
    settings.setValue("InHiddenFolders", ui->checkBoxInHiddenFolders->isChecked());

    settings.endGroup();
}

void FindReplaceDialog::savePosition()
{
    qInfo(Q_FUNC_INFO);

    position = pos();
}

void FindReplaceDialog::restorePosition()
{
    qInfo(Q_FUNC_INFO);

    move(position);
}

int FindReplaceDialog::computeSearchFlags()
{
    int flags = 0;

    if (ui->checkBoxMatchWholeWord->isChecked())
        flags |= SCFIND_WHOLEWORD;
    if (ui->checkBoxMatchCase->isChecked())
        flags |= SCFIND_MATCHCASE;
    if (ui->radioRegexSearch->isChecked())
        flags |= SCFIND_REGEXP;

    return flags;
}

void FindReplaceDialog::showMessage(const QString &message, const QString &color)
{
    statusBar->setStyleSheet(QStringLiteral("color: %1").arg(color));
    statusBar->showMessage(message);
}

void FindReplaceDialog::selectFolderDialog()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder"), QString(), QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty()) {
        updateDirectoryList(dir);
    }
}

// FIXME: =================================== 

bool FindReplaceDialog::findInFiles()
{
    qInfo(Q_FUNC_INFO);

    std::vector<QString> fileNames;
    if (!createFilelistForFiles(fileNames)) {
        return false;
    }
    return findInFilelist(fileNames);
}

bool FindReplaceDialog::createFilelistForFiles(std::vector<QString> & fileNames)
{
    QDir dir{directoryString()};

    if (!dir.exists())
	{
		return false;
	}

    Patterns patterns2Match;
    getAndValidatePatterns(patterns2Match);

    QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::CaseSensitive;

    bool isInHiddenDir = ui->checkBoxInHiddenFolders->isChecked();
    if (isInHiddenDir) {
        filters |= QDir::Hidden;
    }

    bool isRecursive = ui->checkBoxInAllSubfolders->isChecked();
    if (isRecursive) {
        filters |= QDir::AllDirs;
    }

    dir.setFilter(filters);

    getMatchedFileNames(dir, 0, patterns2Match, fileNames);
	return true;
}

void FindReplaceDialog::getAndValidatePatterns(Patterns &patterns)
{
    getPatterns(patterns);
    if (patterns.size() == 0)
    {
        updateFiltersList(QString("*.*"));
        getPatterns(patterns);
    }
    else if (allPatternsAreExclusion(patterns))
    {
        patterns.insert(patterns.begin(), QString("*.*"));
    }
}

void FindReplaceDialog::getPatterns(Patterns &patterns)
{
    cutString(filterString(), patterns);
}

QString getSubstring(const QString &s, const QString::ConstIterator& first, const QString::ConstIterator& last)
{
    // Constructs the string with the contents of the range [first, last).
    return s.sliced(first - s.begin(), last - first);
}

void FindReplaceDialog::cutString(const QString &str2cut, Patterns &patterns)
{
    if (str2cut.isNull())
    {
        return;
    }

    auto itBegin = str2cut.begin();
    auto itEnd = itBegin;

    while (*itEnd != '\0')
    {
        if (itEnd->isSpace())
        {
            if (itBegin != itEnd)
            {
                patterns.emplace_back(getSubstring(str2cut, itBegin, itEnd));
            }
            itBegin = itEnd + 1;
		
        }
        ++itEnd;
    }

    if (itBegin != itEnd)
    {
        patterns.emplace_back(getSubstring(str2cut, itBegin, itEnd));
    }
}

bool FindReplaceDialog::allPatternsAreExclusion(const Patterns &patterns)
 {
    bool oneInclusionPatternFound = false;
    for (size_t i = 0, len = patterns.size(); i < len; ++i)
    {
        if (patterns[i].length() > 1 && patterns[i][0] != '!')
        {
            oneInclusionPatternFound = true;
            break;
        }
    }
    return !oneInclusionPatternFound;
 }

void FindReplaceDialog::getMatchedFileNames(const QDir &dir, size_t level, const Patterns &patterns, std::vector<QString> &fileNames)
{
    ++level;

    for (auto& elem : dir.entryInfoList()) {
        if (elem.isDir()) {
                if (!matchInExcludeDirList(elem.fileName(), patterns, level)) {
                    QDir subDir = QDir(elem.filePath());
                    subDir.setFilter(dir.filter());
                    getMatchedFileNames(subDir, level, patterns, fileNames);
                }
        }
        else {
            if (matchInList(elem.fileName(), patterns)) {
                fileNames.push_back(elem.filePath());
            }
        }
    }
}

// FIXME: Make && version (for pattern arg)?
bool FindReplaceDialog::isPatternMatch(const QString& s, QString pattern) {
    QRegularExpression regex = QRegularExpression::fromWildcard(pattern, Qt::CaseSensitive);
    return regex.match(s).hasMatch();
};

bool FindReplaceDialog::matchInExcludeDirList(const QString &dirName, const Patterns &patterns, size_t level)
{
    for (size_t i = 0, len = patterns.size(); i < len; ++i)
    {
        size_t patterLen = patterns[i].length();

        if (patterLen > 3 && patterns[i][0] == '!' && patterns[i][1] == '+' && patterns[i][2] == '\\')
        // check for !+\folderPattern: for all levels - search this pattern recursively
        {
            if (isPatternMatch(dirName, patterns[i].sliced(3))) {
                return true;
            }
        }
        else if (patterLen > 2 && patterns[i][0] == '!' && patterns[i][1] == '\\')
        // check for !\folderPattern: exclusive pattern for only the 1st level
        {
            if (level == 1) {
                if (isPatternMatch(dirName, patterns[i].sliced(2))) {
                    return true;
                }
            }
        }
    }
    return false;
}

 bool FindReplaceDialog::matchInList(const QString& fileName, const Patterns &patterns)
 {
    bool isMatched = false;
    for (size_t i = 0, len = patterns.size(); i < len; ++i) {
        bool isNegationPattern = patterns[i].length() > 1 && patterns[i][0] == '!';
        if (isNegationPattern) {
            if (isPatternMatch(fileName, patterns[i].sliced(1))) {
                return false;
            }
            continue;
        }

        if (isPatternMatch(fileName, patterns[i])) {
            isMatched = true;
        }
    }
    return isMatched;
 }

 bool FindReplaceDialog::findInFilelist(std::vector<QString> & fileNames)
 {
    qInfo(Q_FUNC_INFO);

//    int nbTotal = 0;
//    ScintillaEditView *pOldView = _pEditView;
//    _pEditView = &_invisibleEditView;
//    Document oldDoc = _invisibleEditView.execute(SCI_GETDOCPOINTER);

//    _findReplaceDlg.beginNewFilesSearch();

    // FIXME: Implement Progress class and its usage here.
    // PowerEditor/src/ScintillaComponent/FindReplaceDlg.h -> class Progress
//    Progress progress(_pPublicInterface->getHinst());

    size_t filesCount = fileNames.size();
    size_t filesPerPercent = 1;

    if (filesCount > 1)
    {
        if (filesCount >= 200)
            filesPerPercent = filesCount / 100;

//        generic_string msg = _nativeLangSpeaker.getLocalizedStrFromID(
//            "find-in-files-progress-title", TEXT("Find In Files progress..."));
//        progress.open(_findReplaceDlg.getHSelf(), msg.c_str());
    }

    const bool isEntireDoc = true;
    bool hasInvalidRegExpr = false;

    ScintillaNext *current_editor = editor;
    for (size_t i = 0, updateOnCount = filesPerPercent; i < filesCount; ++i)
    {
//        if (progress.isCancelled()) break;

        ScintillaNext *editor = ScintillaNext::fromFile(fileNames.at(i), false);
        setEditor(editor);

        findAllInCurrentDocument();
//        editor->close();

//        int nb = _findReplaceDlg.processAll(ProcessFindAll, FindReplaceDlg::_env, isEntireDoc, &findersInfo);

//        if (nb == FIND_INVALID_REGULAR_EXPRESSION)
//        {
//            hasInvalidRegExpr = true;
//            break;
//        }

//        nbTotal += nb;

        if (i == updateOnCount)
        {
            updateOnCount += filesPerPercent;
//            progress.setPercent(int32_t((i * 100) / filesCount), fileNames.at(i).c_str(), nbTotal);
        }
        else
        {
//            progress.setInfo(fileNames.at(i).c_str(), nbTotal);
        }
    }

    setEditor(current_editor);

//    progress.close();

//    _findReplaceDlg.finishFilesSearch(nbTotal, int(filesCount), isEntireDoc);

//    _invisibleEditView.execute(SCI_SETDOCPOINTER, 0, oldDoc);
//    _pEditView = pOldView;

//    _findReplaceDlg.putFindResult(nbTotal);
    // void putFindResult(int result) {
    // 	_findAllResult = result;
    // };

//    if (hasInvalidRegExpr)
//    {
//        _findReplaceDlg.setStatusbarMessageWithRegExprErr(&_invisibleEditView);
//        return false;
//    }

//    if (nbTotal > 0)
//    {
//        NppParameters& nppParam = NppParameters::getInstance();
//        NppGUI& nppGui = nppParam.getNppGUI();
//        if (!nppGui._findDlgAlwaysVisible)
//        {
//            _findReplaceDlg.display(false);
//        }
//    }

    return true;
 }

// void Notepad_plus::setCodePageForInvisibleView(Buffer const *pBuffer)
// {
// 	intptr_t detectedCp = _invisibleEditView.execute(SCI_GETCODEPAGE);
// 	intptr_t cp2set = SC_CP_UTF8;
// 	if (pBuffer->getUnicodeMode() == uni8Bit)
// 	{
// 		cp2set = (detectedCp == SC_CP_UTF8 ? CP_ACP : detectedCp);
// 	}
// 	_invisibleEditView.execute(SCI_SETCODEPAGE, cp2set);
// }

// int FindReplaceDlg::processAll(ProcessOperation op, const FindOption *opt, bool isEntire, const FindersInfo *pFindersInfo, int colourStyleID)
// {
// 	if (op == ProcessReplaceAll && (*_ppEditView)->getCurrentBuffer()->isReadOnly())
// 	{
// 		NativeLangSpeaker *pNativeSpeaker = (NppParameters::getInstance()).getNativeLangSpeaker();
// 		generic_string msg = pNativeSpeaker->getLocalizedStrFromID("find-status-replaceall-readonly", TEXT("Replace All: Cannot replace text. The current document is read only."));
// 		setStatusbarMessage(msg, FSNotFound);
// 		return 0;
// 	}
//
// 	const FindOption *pOptions = opt?opt:_env;
// 	const TCHAR *txt2find = pOptions->_str2Search.c_str();
// 	const TCHAR *txt2replace = pOptions->_str4Replace.c_str();
// 
// 	Sci_CharacterRangeFull cr = (*_ppEditView)->getSelection();
// 	size_t docLength = (*_ppEditView)->execute(SCI_GETLENGTH);
// 
// 	// Default :
// 	//        direction : down
// 	//        begin at : 0
// 	//        end at : end of doc
// 	size_t startPosition = 0;
// 	size_t endPosition = docLength;
// 
// 	bool direction = pOptions->_whichDirection;
// 
// 	//first try limiting scope by direction
// 	if (direction == DIR_UP)
// 	{
// 		startPosition = 0;
// 		endPosition = cr.cpMax;
// 	}
// 	else
// 	{
// 		startPosition = cr.cpMin;
// 		endPosition = docLength;
// 	}
// 
// 	//then adjust scope if the full document needs to be changed
// 	if (op == ProcessCountAll && pOptions->_isInSelection)
// 	{
// 		startPosition = cr.cpMin;
// 		endPosition = cr.cpMax;
// 	}
// 	else if (pOptions->_isWrapAround || isEntire)	//entire document needs to be scanned
// 	{
// 		startPosition = 0;
// 		endPosition = docLength;
// 	}
// 
// 	//then readjust scope if the selection override is active and allowed
// 	if ((pOptions->_isInSelection) && ((op == ProcessMarkAll) || ((op == ProcessReplaceAll || op == ProcessFindAll) && (!isEntire))))
// 		//if selection limiter and either mark all or replace all or find all w/o entire document override
// 	{
// 		startPosition = cr.cpMin;
// 		endPosition = cr.cpMax;
// 	}
// 
// 	if ((op == ProcessMarkAllExt) && (colourStyleID != -1))
// 	{
// 		startPosition = 0;
// 		endPosition = docLength;
// 	}
// 
// 	FindReplaceInfo findReplaceInfo;
// 	findReplaceInfo._txt2find = txt2find;
// 	findReplaceInfo._txt2replace = txt2replace;
// 	findReplaceInfo._startRange = startPosition;
// 	findReplaceInfo._endRange = endPosition;
// 
// 	int nbProcessed = processRange(op, findReplaceInfo, pFindersInfo, pOptions, colourStyleID);
// 
// 	if (nbProcessed == FIND_INVALID_REGULAR_EXPRESSION)
// 		return FIND_INVALID_REGULAR_EXPRESSION;
// 
// 	if (nbProcessed > 0 && op == ProcessReplaceAll && pOptions->_isInSelection)
// 	{
// 		size_t newDocLength = (*_ppEditView)->execute(SCI_GETLENGTH);
// 		endPosition += newDocLength - docLength;
// 		(*_ppEditView)->execute(SCI_SETSELECTION, endPosition, startPosition);
// 		(*_ppEditView)->execute(SCI_SCROLLRANGE, startPosition, endPosition);
// 		if (startPosition == endPosition)
// 		{
// 			const NppGUI& nppGui = (NppParameters::getInstance()).getNppGUI();
// 			if (nppGui._inSelectionAutocheckThreshold != 0)
// 			{
// 				setChecked(IDC_IN_SELECTION_CHECK, false);
// 			}
// 			enableFindDlgItem(IDC_IN_SELECTION_CHECK, false);
// 		}
// 	}
// 	return nbProcessed;
// }

// void Finder::finishFilesSearch(int count, int searchedCount, bool isMatchLines, bool searchedEntireNotSelection)
// {
// 	std::vector<FoundInfo>* _pOldFoundInfos;
// 	std::vector<SearchResultMarkingLine>* _pOldMarkings;
// 	_pOldFoundInfos = _pMainFoundInfos == &_foundInfos1 ? &_foundInfos2 : &_foundInfos1;
// 	_pOldMarkings = _pMainMarkings == &_markings1 ? &_markings2 : &_markings1;
// 
// 	_pOldFoundInfos->insert(_pOldFoundInfos->begin(), _pMainFoundInfos->begin(), _pMainFoundInfos->end());
// 	_pOldMarkings->insert(_pOldMarkings->begin(), _pMainMarkings->begin(), _pMainMarkings->end());
// 	_pMainFoundInfos->clear();
// 	_pMainMarkings->clear();
// 	_pMainFoundInfos = _pOldFoundInfos;
// 	_pMainMarkings = _pOldMarkings;
//
// 	_markingsStruct._length = static_cast<long>(_pMainMarkings->size());
// 	if (_pMainMarkings->size() > 0)
// 		_markingsStruct._markings = &((*_pMainMarkings)[0]);
//
// 	addSearchHitCount(count, searchedCount, isMatchLines, searchedEntireNotSelection);
// 	_scintView.execute(SCI_SETSEL, 0, 0);
//
// 	//SCI_SETILEXER resets the lexer property @MarkingsStruct and then no data could be exchanged with the searchResult lexer
// 	char ptrword[sizeof(void*) * 2 + 1];
// 	sprintf(ptrword, "%p", static_cast<void*>(&_markingsStruct));
// 	_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("@MarkingsStruct"), reinterpret_cast<LPARAM>(ptrword));
// 
// 	//previous code: _scintView.execute(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(CreateLexer("searchResult")));
// 	_scintView.execute(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("fold"), reinterpret_cast<LPARAM>("1"));
// 
// 	_previousLineNumber = -1;
// }
