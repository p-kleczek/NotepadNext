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


#ifndef FINDREPLACEDIALOG_H
#define FINDREPLACEDIALOG_H

#include <QDialog>
#include <QEvent>
#include <QStatusBar>
#include <QTabBar>
#include <vector>

#include "Finder.h"
#include "ISearchResultsHandler.h"


class ScintillaNext;
class MainWindow;

namespace Ui {
class FindReplaceDialog;
}

class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    enum {
        FIND_TAB = 0,
        REPLACE_TAB = 1,
        FIND_IN_FILES_TAB = 2,
        MARK_TAB = 3
    };

    explicit FindReplaceDialog(ISearchResultsHandler *searchResults, MainWindow *window = nullptr);
    ~FindReplaceDialog() override;

    void setFindString(const QString &string);
    void setTab(int tab);

    QString replaceString();

    void setSearchResultsHandler(ISearchResultsHandler *searchResultsHandler);

protected:
    bool event(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

signals:
    void windowActivated();
    void windowDeactivated();

public slots:
    void performLastSearch();

    void find();
    void findAllInCurrentDocument();
    void findAllInDocuments();
    void count();
    void replace();
    void replaceAll();

private slots:
    void setEditor(ScintillaNext *edit);
    void adjustOpacity(int value);
    void transparencyToggled(bool on);
    void adjustOpacityWhenLosingFocus(bool checked);
    void adjustOpacityAlways(bool on);

    void changeTab(int index);

private:
    QString findString();
    QString filterString();
    QString directoryString();
    
    void prepareToPerformSearch(bool replace=false);
    void loadSettings();
    void saveSettings();

    void savePosition();
    void restorePosition();

    int computeSearchFlags();

    void showMessage(const QString &message, const QString &color);

    void updateFindList(const QString &text);
    void updateReplaceList(const QString &text);
    void updateFiltersList(const QString &text);
    void updateDirectoryList(const QString &text);

    using Patterns = std::vector<QString>;

    void selectFolderDialog();
    bool findInFiles();
    bool createFilelistForFiles(std::vector<QString> &fileNames);
    void getAndValidatePatterns(Patterns &patterns);
    void getPatterns(Patterns &patterns);
    void cutString(const QString &str2cut, Patterns& patterns);
    bool allPatternsAreExclusion(const Patterns &patterns);
    void getMatchedFileNames(const QDir &dir, size_t level, const Patterns &patterns, std::vector<QString> & fileNames);
    bool isPatternMatch(const QString& s, QString pattern);
    bool matchInExcludeDirList(const QString &dirName, const Patterns &patterns, size_t level);
    bool matchInList(const QString& fileName, const Patterns &patterns);
    bool findInFilelist(std::vector<QString> & fileNames);

    bool isFirstTime = true;
    QPoint position;
    Ui::FindReplaceDialog *ui;

    ScintillaNext *editor;
    QStatusBar *statusBar;
    QTabBar *tabBar;

    ISearchResultsHandler *searchResultsHandler;
    Finder *finder;
};

#endif // FINDREPLACEDIALOG_H
