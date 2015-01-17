#include "AirsyncDownloadResource.hxx"
#include "SettingsDialog.hxx"

#include "settings.h"
#include "settingsadaptor.h"

#include <specialmailcollections.h>

#include <QtDBus/QDBusConnection>

using namespace Akonadi;

//--------------------------------------------------------------------------------

AirsyncDownloadResource::AirsyncDownloadResource(const QString &id)
  : ResourceBase(id), state(Idle)
{
  new SettingsAdaptor(Settings::self());
  QDBusConnection::sessionBus()
    .registerObject(QLatin1String("/Settings"),
                    Settings::self(), QDBusConnection::ExportAdaptors);

  setNeedsNetwork(true);

  intervalTimer = new QTimer(this);
  intervalTimer->setSingleShot(true);
  connect(intervalTimer, SIGNAL(timeout()), this, SLOT(startMailCheck()));

  updateIntervalTimer();

  if ( SpecialMailCollections::self()->hasDefaultCollection(SpecialMailCollections::Inbox) )
    targetCollection = SpecialMailCollections::self()->defaultCollection(SpecialMailCollections::Inbox);
  else
  {
    // TODO setup SpecialMailCollectionsRequestJob
  }
}

//--------------------------------------------------------------------------------

AirsyncDownloadResource::~AirsyncDownloadResource()
{
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::retrieveCollections()
{
  // TODO: this method is called when Akonadi wants to have all the
  // collections your resource provides.
  // Be sure to set the remote ID and the content MIME types

  if ( state == Idle )
    startMailCheck();
  else
  {
    cancelSync(i18n("Mail check already in progress, unable to start a second check."));
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::retrieveItems( const Akonadi::Collection &collection )
{
  Q_UNUSED( collection );

  // TODO: this method is called when Akonadi wants to know about all the
  // items in the given collection. You can but don't have to provide all the
  // data for each item, remote ID and MIME type are enough at this stage.
  // Depending on how your resource accesses the data, there are several
  // different ways to tell Akonadi when you are done.
}

//--------------------------------------------------------------------------------

bool AirsyncDownloadResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( item );
  Q_UNUSED( parts );

  // TODO: this method is called when Akonadi wants more data for a given item.
  // You can only provide the parts that have been requested but you are allowed
  // to provide all in one go

  return true;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::aboutToQuit()
{
  // TODO: any cleanup you need to do while there is still an active
  // event loop. The resource will terminate after this method returns
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::configure( WId windowId )
{
  QPointer<SettingsDialog> dialog = new SettingsDialog(this, windowId);

  if ( dialog->exec() == QDialog::Accepted )
  {
    updateIntervalTimer();
    emit configurationDialogAccepted();
  }
  else
    emit configurationDialogRejected();

  delete dialog;
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  Q_UNUSED( item );
  Q_UNUSED( collection );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has created an item in a collection managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( item );
  Q_UNUSED( parts );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has changed an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::itemRemoved( const Akonadi::Item &item )
{
  Q_UNUSED( item );

  // TODO: this method is called when somebody else, e.g. a client application,
  // has deleted an item managed by your resource.

  // NOTE: There is an equivalent method for collections, but it isn't part
  // of this template code to keep it simple
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::updateIntervalTimer()
{
  if ( Settings::self()->intervalCheckEnabled() && (state == Idle) )
  {
    intervalTimer->start(Settings::self()->intervalCheckInterval() * 1000 * 60);
  }
  else
  {
    intervalTimer->stop();
  }
}

//--------------------------------------------------------------------------------

void AirsyncDownloadResource::startMailCheck()
{
  emit percent(0); // Otherwise the value from the last sync is taken
  emit status(Running);
}

//--------------------------------------------------------------------------------

AKONADI_RESOURCE_MAIN( AirsyncDownloadResource )

#include "AirsyncDownloadResource.moc"
