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

#include <AirsyncDownloadResource.hxx>
#include <SettingsDialog.hxx>
#include <Session.hxx>

#include "settings.h"
#include "settingsadaptor.h"

#include <akonadi/kmime/specialmailcollections.h>
#include <akonadi/kmime/messageflags.h>
#include <kwallet.h>

#include <QtDBus/QDBusConnection>
#include <QUuid>

//--------------------------------------------------------------------------------

AirsyncDownloadResource::AirsyncDownloadResource(const QString &id)
  : ResourceBase(id), session(0), downloadFinished(false)
{
  setNeedsNetwork(true);

  new SettingsAdaptor(Settings::self());
  QDBusConnection::sessionBus()
    .registerObject(QLatin1String("/Settings"),
                    Settings::self(), QDBusConnection::ExportAdaptors);

  QString deviceId = Settings::self()->deviceId();
  if ( deviceId.isEmpty() )  // first time usage - create a unique device id
  {
    Settings::self()->setDeviceId(QUuid::createUuid().toString().replace('-', "").mid(1, 32));
    Settings::self()->writeConfig();
  }

  intervalTimer = new QTimer(this);
  intervalTimer->setSingleShot(false);
  connect(intervalTimer, SIGNAL(timeout()), this, SLOT(startMailCheck()));

  loadConfiguration();
  connect(this, SIGNAL(reloadConfiguration()), this, SLOT(loadConfiguration()));

  connect(this, SIGNAL(abortRequested()), this, SLOT(slotAbortRequested()));

  setAutomaticProgressReporting(false);
}

//--------------------------------------------------------------------------------

AirsyncDownloadResource::~AirsyncDownloadResource()
{
  delete session;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::retrieveCollections()
{
  if ( status() == NotConfigured )
  {
    cancelTask(i18n("The resource is not configured correctly and can not work."));
    return;
  }

  if ( !isOnline() )
  {
    cancelTask(i18n("The resource is offline."));
    return;
  }

  if ( status() != Running )
    startMailCheck();
  else
  {
    cancelTask(i18n("Mail check already in progress, unable to start a second check."));
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::retrieveItems(const Akonadi::Collection &collection)
{
  Q_UNUSED( collection );

  kWarning() << "This should never be called, we don't have a collection!";
}

//--------------------------------------------------------------------------------

bool AirsyncDownloadResource::retrieveItem(const Akonadi::Item &item, const QSet<QByteArray> &parts)
{
  Q_UNUSED( item );
  Q_UNUSED( parts );

  kWarning() << "This should never be called, we don't have any item!";
  return false;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::aboutToQuit()
{
  if ( status() == Running )
  {
    session->abortFetching();
    cancelTask(i18n("Mail check aborted."));
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::slotAbortRequested()
{
  if ( status() == Running )
  {
    session->abortFetching();
    cancelTask(i18n("Mail check was canceled manually."));
    emit status(Idle, i18n("Ready"));
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::configure(WId windowId)
{
  QPointer<SettingsDialog> dialog = new SettingsDialog(this, windowId);

  if ( dialog->exec() == QDialog::Accepted )
  {
    loadConfiguration();
    emit configurationDialogAccepted();
  }
  else
  {
    emit configurationDialogRejected();
  }

  delete dialog;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::loadConfiguration()
{
  if ( status() != Idle )  // else we would delete session which is currently in use
    return;

  delete session;

  Settings::self()->readConfig();

  if ( Settings::self()->intervalCheckEnabled() )
  {
    intervalTimer->start(Settings::self()->intervalCheckInterval() * 1000 * 60);
  }
  else
  {
    intervalTimer->stop();
  }

  targetCollection = Akonadi::Collection(Settings::self()->targetCollection());
  if ( !targetCollection.isValid() )
  {
    emit status(NotConfigured, i18n("The configured mail target folder is not valid"));
  }

  KWallet::Wallet *wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), winIdForDialogs());
  if ( wallet )
  {
    if ( wallet->setFolder(KWallet::Wallet::PasswordFolder()) )
      wallet->readPassword(identifier(), password);

    delete wallet;
  }

  QString server;
  if ( Settings::self()->secureHttp() )
    server = QLatin1String("https://");
  else
    server = QLatin1String("http://");

  server += Settings::self()->server();

  session = new Session(server, Settings::self()->domain(),
                        Settings::self()->userName(), password,
                        Settings::self()->deviceId());

  // hidden propertry, probably needed when some UA filter rule on the server changes
  session->setUserAgent(Settings::self()->userAgent().toUtf8());

  connect(session, SIGNAL(progress(int)), this, SIGNAL(percent(int)));
  connect(session, SIGNAL(newMessage(KMime::Message::Ptr, QByteArray)),
          this, SLOT(newMessage(KMime::Message::Ptr, QByteArray)));
  connect(session, SIGNAL(errorMessage(QString)), this, SLOT(errorMessageChanged(QString)));
  connect(session, SIGNAL(authRequired()), this, SLOT(authRequired()));
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::errorMessageChanged(const QString &msg)
{
  lastErrorMessage = msg;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::authRequired()
{
  QString msg = i18n("The server authentication failed. Check username and password.");
  cancelTask(msg);
  emit status(NotConfigured, msg);
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::startMailCheck()
{
  if ( !isOnline() ||
       (status() == Running) ||        // previous check still in progress
       (status() == NotConfigured) )   // e.g. authentication error or targetCollection invalid
    return;

  emit status(Running, i18n("Checking for new mails"));

  mailsToDelete.clear();
  pendingCreateJobs.clear();
  downloadFinished = false;

  int ret = session->fetchMails();
  if ( ret < 0 )
  {
    // e.g. the server sends "Retry after sending a PROVISION command"
    // let's try one more time (Session has already reset its policyKey)
    if ( status() == Running )  // could already be NotConfigured (failed auth)
      ret = session->fetchMails();

    if ( ret < 0 )
    {
      QString msg = lastErrorMessage.isEmpty() ? i18n("Could not fetch mails from server") : lastErrorMessage;
      if ( status() == Running )  // could already be NotConfigured (failed auth)
      {
        emit status(Broken, msg);
      }
      cancelTask(msg);
      return;
    }
  }

  downloadFinished = true;

  if ( pendingCreateJobs.isEmpty() )  // no mail or all mails created
    finish();
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::finish()
{
  if ( !mailsToDelete.isEmpty() )
  {
    emit status(Running, i18n("Deleting mails from the server."));

    if ( session->deleteMails(mailsToDelete) < 0 )
    {
      QString msg = lastErrorMessage.isEmpty() ? i18n("Could not delete mails from the server") : lastErrorMessage;
      emit status(Broken, msg);
      cancelTask(msg);
      return;
    }
  }

  collectionsRetrieved(Akonadi::Collection::List());

  int num = mailsToDelete.count();

  if ( num == 0 )
    emit status(Idle, i18n("Finished mail check, no mails downloaded."));
  else
  {
    emit status(Idle, i18np("Finished mail check, 1 mail downloaded.",
                            "Finished mail check, %1 mails downloaded.", num));
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::newMessage(KMime::Message::Ptr message, const QByteArray &mailId)
{
  if ( status() != Running )  // some error occured during item create jobs
    return;

  Akonadi::Item item;
  item.setMimeType(QLatin1String("message/rfc822"));  // mail
  item.setPayload<KMime::Message::Ptr>(message);

  // TODO: change to following when included in upstream code
  // Akonadi::MessageFlags::copyMessageFlags(*message, item);

  // update status flags
  if ( KMime::isSigned(message.get()) )
    item.setFlag(Akonadi::MessageFlags::Signed);

  if ( KMime::isEncrypted(message.get()) )
    item.setFlag(Akonadi::MessageFlags::Encrypted);

  if ( KMime::isInvitation(message.get()) )
    item.setFlag(Akonadi::MessageFlags::HasInvitation);

  if ( KMime::hasAttachment(message.get()) )
    item.setFlag(Akonadi::MessageFlags::HasAttachment);

  Akonadi::ItemCreateJob *itemCreateJob = new Akonadi::ItemCreateJob(item, targetCollection);

  pendingCreateJobs.insert(itemCreateJob, mailId);
  connect(itemCreateJob, SIGNAL(result(KJob *)), this, SLOT(itemCreateJobResult(KJob *)));
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::itemCreateJobResult(KJob *job)
{
  QByteArray mailId = pendingCreateJobs.value(job);
  pendingCreateJobs.remove(job);

  if ( job->error() )
  {
    QString msg = i18n("Unable to store downloaded mails." ) +
                  QLatin1Char('\n') + job->errorString();
    emit status(Broken, msg);
    cancelTask(msg);
    return;
  }

  mailsToDelete.append(mailId);

  if ( downloadFinished )
    finish();
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::doSetOnline(bool online)
{
  ResourceBase::doSetOnline(online);

  if ( !online && (status() == Running) )
  {
    session->abortFetching();
    cancelTask(i18n("Mail check aborted after going offline."));
  }
}

//--------------------------------------------------------------------------------

AKONADI_RESOURCE_MAIN( AirsyncDownloadResource )

#include "AirsyncDownloadResource.moc"
