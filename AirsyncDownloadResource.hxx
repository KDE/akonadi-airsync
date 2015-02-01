#ifndef AIRSYNCDOWNLOADRESOURCE_H
#define AIRSYNCDOWNLOADRESOURCE_H

#include <akonadi/resourcebase.h>
#include <Akonadi/ItemCreateJob>
#include <QTimer>
#include <KMime/Message>
class Session;

class AirsyncDownloadResource : public Akonadi::ResourceBase,
                                public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    AirsyncDownloadResource(const QString &id);
    virtual ~AirsyncDownloadResource();

  public Q_SLOTS:
    virtual void configure(WId windowId);

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems(const Akonadi::Collection &col);
    bool retrieveItem(const Akonadi::Item &item, const QSet<QByteArray> &parts);

  protected:
    virtual void aboutToQuit();
    virtual void doSetOnline(bool online);

  private Q_SLOTS:
    void loadConfiguration();
    void startMailCheck();
    void newMessage(KMime::Message::Ptr message, const QByteArray &mailId);
    void itemCreateJobResult(KJob *job);
    void errorMessageChanged(const QString &msg);
    void authRequired();
    void slotAbortRequested();

  private:  // methods
    void finish();

  private:  // members
    QTimer *intervalTimer;
    Akonadi::Collection targetCollection;
    QString password; // from wallet
    Session *session;
    QMap<KJob *, QByteArray> pendingCreateJobs;
    QList<QByteArray> mailsToDelete;
    bool downloadFinished;
    QString lastErrorMessage;
};

#endif
