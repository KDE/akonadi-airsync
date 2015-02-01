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

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <KMime/Message>

class Session : public QObject
{
  Q_OBJECT

  public:
    Session(const QString &server, const QString &domain,
            const QString &username, const QString &password, const QString &uniqueDeviceId);

    QByteArray sendCommand(const QByteArray &cmd, const QByteArray &data);

    QByteArray post(QNetworkRequest req, const QByteArray &data);

    // get OPTIONS and print them
    void options();

    static bool dataToWbXml(const QByteArray &data, QByteArray &result);
    static bool dataFromWbXml(const QByteArray &data, QByteArray &result);


    void setUserAgent(const QByteArray &ua) { userAgent = ua; }

    int fetchMails();
    int deleteMails(const QList<QByteArray> &mailIds);

    void abortFetching();

  signals:
    void progress(int);
    void newMessage(KMime::Message::Ptr message, const QByteArray &mailId);
    void errorMessage(const QString &msg);
    void authRequired();

  private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

  private:
    int init();
    void setPolicyKey(uint key) { policyKey = key; }
    static int debugArea();
    static int debugArea2();

    QNetworkAccessManager *nam;
    QEventLoop *eventLoop;

    QUrl serverUrl;
    QByteArray loginBase64;
    QByteArray userAgent;
    QByteArray collectionId;
    QByteArray collectionSyncKey;
    uint policyKey;
    bool wroteSslWarning;
    bool aborted;
    bool errorOccured;
};
