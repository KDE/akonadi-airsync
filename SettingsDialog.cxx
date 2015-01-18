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
  ui.intervalSpin->setRange(1, 10000, 1);

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

  wallet = Wallet::openWallet(Wallet::NetworkWallet(), winId());
  if ( wallet )
  {
    if ( !wallet->hasFolder(Wallet::PasswordFolder()) )
      wallet->createFolder(Wallet::PasswordFolder());

    wallet->setFolder(Wallet::PasswordFolder());  // set current folder

    QString password;
    if ( wallet->readPassword(resource->identifier(), password) == 0 )
      ui.passwordEdit->setText(password);
  }
  else
  {
    ui.passwordEdit->setClickMessage(i18n("Wallet disabled in system settings"));
  }

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
      accept();
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

  Settings::self()->setTargetCollection(ui.folderRequester->collection().id());
  Settings::self()->writeConfig();

  if ( wallet )
  {
    wallet->writePassword(resource->identifier(), ui.passwordEdit->text());
  }
}

//--------------------------------------------------------------------------------
