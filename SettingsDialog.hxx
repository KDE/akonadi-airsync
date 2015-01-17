#ifndef _SettingsDialog_H_
#define _SettingsDialog_H_

#include <KDialog>
namespace KWallet
{
  class Wallet;
}
#include <QRegExpValidator>

#include <ui_Settings.h>

class AirsyncDownloadResource;
class KJob;

//--------------------------------------------------------------------------------

class SettingsDialog : public KDialog
{
  Q_OBJECT

  public:
    SettingsDialog(AirsyncDownloadResource *resource, WId parentWindow);
    virtual ~SettingsDialog();

  private slots:
    void targetCollectionReceived(Akonadi::Collection::List collections);
    void localFolderRequestJobFinished(KJob *job);
    void walletOpenedForLoading(bool success);
    void walletOpenedForSaving(bool success);
    void showPasswordChecked(bool checked);
    virtual void slotButtonClicked(int button);

  private:  // methods
    void loadSettings();
    void saveSettings();

  private:
    AirsyncDownloadResource *resource;
    Ui::Settings ui;
    QRegExpValidator validator;
    KWallet::Wallet *wallet;
    QString initialPassword;
};

#endif
