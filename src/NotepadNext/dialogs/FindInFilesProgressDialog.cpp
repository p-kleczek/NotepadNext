#include "FindInFilesProgressDialog.h"
#include "ui_FindInFilesProgressDialog.h"

FindInFilesProgressDialog::FindInFilesProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FindInFilesProgressDialog)
{
    ui->setupUi(this);
    // FIXME: Choose proper modality.
    setModal(true);
//    setWindowModality(Qt::WindowModal);
}

FindInFilesProgressDialog::~FindInFilesProgressDialog()
{
    delete ui;
}

void FindInFilesProgressDialog::setPercent(int percent, const QString& fileName, int nbHitsSoFar)
{
    ui->progressBar->setValue(percent);
    ui->labelFilePath->setText(fileName);
    // FIXME: Implement nbHitsSoFar (see Progress::setPercent)
}
void FindInFilesProgressDialog::setInfo(const QString& info, int nbHitsSoFar)
{
    ui->labelFilePath->setText(info);
    // FIXME: Implement nbHitsSoFar (see Progress::setInfo)
}
