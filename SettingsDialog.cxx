#include <SettingsDialog.hxx>
#include <AirsyncDownloadResource.hxx>
#include "settings.h"

#include <Akonadi/Collection>
#include <Akonadi/CollectionFetchJob>
#include <akonadi/kmime/specialmailcollections.h>
#include <akonadi/kmime/specialmailcollectionsrequestjob.h>
#include <akonadi/resourcesettings.h>

#include <KWindowSystem>
#include <KUser>
#include <kwallet.h>

#include <QUuid>

using namespace Akonadi;
using namespace KWallet;

//--------------------------------------------------------------------------------

SettingsDialog::SettingsDialog(AirsyncDownloadResource *res, WId parentWindow)
  : resource(res), wallet(0)
{
  ui.setupUi(mainWidget());

  KWindowSystem::setMainWindow(this, parentWindow);
  setWindowIcon(KIcon("network-server"));
  setWindowTitle(i18n("AirSyncDownload Account Settings"));
  setButtons(Ok|Cancel);

  // only letters, digits, '-', '.', ':' (IPv6) and '_' (for Windows
  // compatibility) are allowed
  validator.setRegExp(QRegExp("[A-Za-z0-9-_:.]*"));
  ui.serverEdit->setValidator(&validator);

  ui.intervalSpin->setSuffix(ki18np(" minute", " minutes"));
  ui.intervalSpin->setRange(ResourceSettings::self()->minimumCheckInterval(), 10000, 1);

  ui.folderRequester->setMimeTypeFilter(QStringList() << QLatin1String("message/rfc822"));
  ui.folderRequester->setFrameStyle(QFrame::NoFrame);
  ui.folderRequester->setAccessRightsFilter(Akonadi::Collection::CanCreateItem);
  ui.folderRequester->changeCollectionDialogOptions(Akonadi::CollectionDialog::AllowToCreateNewChildCollection);

  loadSettings();
}

//--------------------------------------------------------------------------------

SettingsDialog::~SettingsDialog()
{
  delete wallet;
}

//--------------------------------------------------------------------------------

void SettingsDialog::loadSettings()
{
  QString deviceId = Settings::self()->deviceId();
  if ( deviceId.isEmpty() )  // first time usage - create a unique device id
    Settings::self()->setDeviceId(QUuid::createUuid().toString().replace('-', "").mid(1, 32));

  ui.nameEdit->setText(resource->name());
  ui.nameEdit->setFocus();

  ui.serverEdit->setText(Settings::self()->server());
  ui.httpsCheck->setChecked(Settings::self()->secureHttp());
  ui.domainEdit->setText(Settings::self()->domain());

  ui.userNameEdit->setText(Settings::self()->userName().length() ? Settings::self()->userName() :
                           KUser().loginName());

  ui.intervalCheck->setChecked(Settings::self()->intervalCheckEnabled());
  ui.intervalSpin->setValue(Settings::self()->intervalCheckInterval());
  ui.intervalSpin->setEnabled(Settings::self()->intervalCheckEnabled());
  ui.leaveOnServerCheck->setChecked(Settings::self()->leaveMailsOnServer());

  // We need to fetch the collection, as the CollectionRequester needs the name
  // of it to work correctly
  Collection targetCollection(Settings::self()->targetCollection());
  if ( targetCollection.isValid() )
  {
    CollectionFetchJob *fetchJob = new CollectionFetchJob(targetCollection,
                                                          CollectionFetchJob::Base,
                                                          this );
    connect(fetchJob, SIGNAL(collectionsReceived(Akonadi::Collection::List)),
            this, SLOT(targetCollectionReceived(Akonadi::Collection::List)));
  }
  else
  {
    // No target collection set in the config? Try requesting a default inbox
    SpecialMailCollectionsRequestJob *requestJob = new SpecialMailCollectionsRequestJob(this);
    requestJob->requestDefaultCollection(SpecialMailCollections::Inbox);
    requestJob->start();
    connect(requestJob, SIGNAL(result(KJob*)),
             this, SLOT(localFolderRequestJobFinished(KJob*)));
  }

  wallet = Wallet::openWallet(Wallet::NetworkWallet(), winId(), Wallet::Asynchronous);
  if ( wallet )
  {
    connect(wallet, SIGNAL(walletOpened(bool)),
            this, SLOT(walletOpenedForLoading(bool)));
  }
  else
  {
    ui.passwordEdit->setClickMessage(i18n("Wallet disabled in system settings"));
  }
  ui.passwordEdit->setEnabled(false);
  ui.passwordLabel->setEnabled(false);

  connect(ui.showPasswordCheck, SIGNAL(toggled(bool)), this, SLOT(showPasswordChecked(bool)));
}

//--------------------------------------------------------------------------------

void SettingsDialog::targetCollectionReceived(Akonadi::Collection::List collections)
{
  ui.folderRequester->setCollection(collections.first());
}

//--------------------------------------------------------------------------------

void SettingsDialog::localFolderRequestJobFinished(KJob *job)
{
  if ( !job->error() )
  {
    Collection targetCollection = SpecialMailCollections::self()->defaultCollection(SpecialMailCollections::Inbox);
    ui.folderRequester->setCollection(targetCollection);
  }
}

//--------------------------------------------------------------------------------

void SettingsDialog::walletOpenedForLoading(bool success)
{
  if ( success )
  {
    if ( wallet->isOpen() )
    {
      ui.passwordEdit->setEnabled(true);
      ui.passwordLabel->setEnabled(true);
    }

    if ( wallet->isOpen() && wallet->hasFolder("airsync") )
    {
      QString password;
      wallet->setFolder("airsync");
      wallet->readPassword(resource->identifier(), password);
      ui.passwordEdit->setText(password);
      initialPassword = password;
    }
    else
    {
      kWarning() << "Wallet not open or doesn't have airsync folder.";
    }
  }
  else
  {
    kWarning() << "Failed to open wallet for loading the password.";
  }

  if ( !success || !wallet->isOpen() )
  {
    ui.passwordEdit->setClickMessage(i18n("Unable to open wallet"));
  }
}

//--------------------------------------------------------------------------------

void SettingsDialog::showPasswordChecked(bool checked)
{
  if ( checked )
    ui.passwordEdit->setEchoMode(QLineEdit::Normal);
  else
    ui.passwordEdit->setEchoMode(QLineEdit::Password);
}

//--------------------------------------------------------------------------------

void SettingsDialog::slotButtonClicked(int button)
{
  switch( button )
  {
    case Ok:
    {
      saveSettings();

      // Don't call accept() yet, saveSettings() triggers an asnychronous wallet operation,
      // which will call accept() when it is finished
      break;
    }

    case Cancel:
    {
      reject();
      break;
    }
  }
}

//--------------------------------------------------------------------------------

void SettingsDialog::saveSettings()
{
  resource->setName(ui.nameEdit->text());

  Settings::self()->setServer(ui.serverEdit->text().trimmed());
  Settings::self()->setSecureHttp(ui.httpsCheck->isChecked());
  Settings::self()->setDomain(ui.domainEdit->text().trimmed());
  Settings::self()->setUserName(ui.userNameEdit->text().trimmed());

  Settings::self()->setIntervalCheckEnabled(ui.intervalCheck->isChecked());
  Settings::self()->setIntervalCheckInterval(ui.intervalSpin->value());
  Settings::self()->setLeaveMailsOnServer(ui.leaveOnServerCheck->isChecked());

  Settings::self()->setTargetCollection(ui.folderRequester->collection().id());
  Settings::self()->writeConfig();

  // Now, either save the password or delete it from the wallet. For both, we need
  // to open it.
  const bool userChangedPassword = initialPassword != ui.passwordEdit->text();
  const bool userWantsToDeletePassword =
      ui.passwordEdit->text().isEmpty() && userChangedPassword;

  if ( ( !ui.passwordEdit->text().isEmpty() && userChangedPassword ) ||
       userWantsToDeletePassword )
  {
    if ( wallet && wallet->isOpen() )
    {
      // wallet is already open
      walletOpenedForSaving(true);
    }
    else
    {
      // we need to open the wallet
      kDebug() << "we need to open the wallet";
      wallet = Wallet::openWallet(Wallet::NetworkWallet(), winId(),
                                  Wallet::Asynchronous);
      if ( wallet )
      {
        connect(wallet, SIGNAL(walletOpened(bool)),
                this, SLOT(walletOpenedForSaving(bool)));
      }
      else
      {
        accept();
      }
    }
  }
  else
  {
    accept();
  }
}

//--------------------------------------------------------------------------------

void SettingsDialog::walletOpenedForSaving(bool success)
{
  if ( success )
  {
    if ( wallet && wallet->isOpen() )
    {
      // Remove the password from the wallet if the user doesn't want to store it
      if ( ui.passwordEdit->text().isEmpty() && wallet->hasFolder("airsync") )
      {
        wallet->setFolder("airsync");
        wallet->removeEntry(resource->identifier());
      }
      else if ( !ui.passwordEdit->text().isEmpty() )
      {
        // Store the password in the wallet if the user wants that
        if ( !wallet->hasFolder("airsync") )
          wallet->createFolder("airsync");

        wallet->setFolder("airsync");
        wallet->writePassword(resource->identifier(), ui.passwordEdit->text());
      }

      //resource->clearCachedPassword();
    }
    else
    {
      kWarning() << "Wallet not open.";
    }
  }
  else
  {
    // Should we alert the user here?
    kWarning() << "Failed to open wallet for saving the password.";
  }

  delete wallet;
  wallet = 0;
  accept();
}

//--------------------------------------------------------------------------------
