/* Copyright 2015, Martin Koller, kollix@aon.at

  This file is part of airsyncDownload.

  airsyncDownload is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  airsyncDownload is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with airsyncDownload.  If not, see <http://www.gnu.org/licenses/>.
*/

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

    QString getPassword() const { return ui.passwordEdit->text(); }

  private slots:
    void targetCollectionReceived(Akonadi::Collection::List collections);
    void localFolderRequestJobFinished(KJob *job);
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
};

#endif
