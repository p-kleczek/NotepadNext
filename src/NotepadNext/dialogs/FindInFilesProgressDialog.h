#ifndef FINDINFILESPROGRESSDIALOG_H
#define FINDINFILESPROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class FindInFilesProgressDialog;
}

class FindInFilesProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FindInFilesProgressDialog(QWidget *parent = nullptr);
    ~FindInFilesProgressDialog();

public slots:
    void setPercent(int percent, const QString& fileName, int nbHitsSoFar);
    void setInfo(const QString& info, int nbHitsSoFar);

private:
    Ui::FindInFilesProgressDialog *ui;
};

#endif // FINDINFILESPROGRESSDIALOG_H
