#ifndef AIRSYNCDOWNLOADRESOURCE_H
#define AIRSYNCDOWNLOADRESOURCE_H

#include <akonadi/resourcebase.h>
#include <QTimer>

class AirsyncDownloadResource : public Akonadi::ResourceBase,
                                public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    AirsyncDownloadResource(const QString &id);
    ~AirsyncDownloadResource();

  public Q_SLOTS:
    virtual void configure(WId windowId);

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems(const Akonadi::Collection &col);
    bool retrieveItem(const Akonadi::Item &item, const QSet<QByteArray> &parts);

  protected:
    virtual void aboutToQuit();

    virtual void itemAdded(const Akonadi::Item &item, const Akonadi::Collection &collection);
    virtual void itemChanged(const Akonadi::Item &item, const QSet<QByteArray> &parts);
    virtual void itemRemoved(const Akonadi::Item &item);

  private Q_SLOTS:
    void startMailCheck();

  private:  // methods
    void updateIntervalTimer();

  private:  // members
    QTimer *intervalTimer;
    Akonadi::Collection targetCollection;

    enum State
    {
      Idle,
    } state;
};

#endif
