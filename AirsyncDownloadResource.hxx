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

#ifndef AIRSYNCDOWNLOADRESOURCE_H
#define AIRSYNCDOWNLOADRESOURCE_H

#include <AkonadiAgentBase/ResourceBase>
#include <AkonadiCore/ItemCreateJob>
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
    void startMailCheck(bool fromTimer = true);
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
    bool mailCheckFromTimer;
    bool downloadFinished;
    QString lastErrorMessage;
};

#endif
