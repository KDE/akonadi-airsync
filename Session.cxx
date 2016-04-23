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

#include <Session.hxx>

#include <wbxml.h>
#include <iostream>

#include <QUuid>
#include <QSslError>
#include <QDomDocument>
#include <QDateTime>

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(airSync, "airSync")
Q_LOGGING_CATEGORY(airSync2, "airSync2")

//--------------------------------------------------------------------------------

#define PRINT_DEBUG(x) { qCDebug(airSync) << QDateTime::currentDateTime().toString(Qt::ISODate) << x; }
#define PRINT_DEBUG2(x) { qCDebug(airSync2) << QDateTime::currentDateTime().toString(Qt::ISODate) << x; }

//--------------------------------------------------------------------------------

Session::Session(const QString &server, const QString &domain,
                 const QString &username, const QString &password,
                 const QString &uniqueDeviceId)
  : serverUrl(server), policyKey(0), wroteSslWarning(false), aborted(false), errorOccured(false)
{
  nam = new QNetworkAccessManager(this);
  eventLoop = new QEventLoop(this);

  connect(nam, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError> &)),
          this,  SLOT(sslErrors(QNetworkReply *, const QList<QSslError> &)));

  serverUrl.setPath(QLatin1String("/Microsoft-Server-ActiveSync"));
  // set query parts which stay the same
  serverUrl.addQueryItem("User", (domain + '\\' + username).toUtf8().toPercentEncoding());
  serverUrl.addQueryItem("DeviceId", uniqueDeviceId);
  serverUrl.addQueryItem("DeviceType", "SP");

  loginBase64 = (domain + '\\' + username + ':' + password).toUtf8().toBase64();
}

//--------------------------------------------------------------------------------

QByteArray Session::sendCommand(const QByteArray &cmd, const QByteArray &data)
{
  QNetworkRequest req;

  QUrl url = serverUrl;
  url.addQueryItem(QLatin1String("Cmd"), QString::fromUtf8(cmd));
  req.setUrl(url);

  return post(req, data);
}

//--------------------------------------------------------------------------------

QByteArray Session::post(QNetworkRequest req, const QByteArray &data)
{
  const QByteArray xmlHeader =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<!DOCTYPE ActiveSync PUBLIC \"-//MICROSOFT//DTD ActiveSync//EN\" \"http://www.microsoft.com/\">\n";

  PRINT_DEBUG("------------------------- posting");
  PRINT_DEBUG(req.url().toString());

  req.setRawHeader("Authorization", QByteArray("Basic ") + loginBase64);
  req.setRawHeader("MS-ASProtocolVersion", "12.1");
  req.setRawHeader("User-Agent", userAgent.isEmpty() ? QByteArray("TouchDown(MSRPC)/8.4.00086/") : userAgent);

  if ( policyKey > 0 )
    req.setRawHeader("X-MS-PolicyKey", QByteArray::number(policyKey));

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/vnd.ms-sync.wbxml");
  QByteArray wbxml;
  if ( !dataToWbXml(xmlHeader + data, wbxml) )
    return QByteArray();

  if ( airSync2().isDebugEnabled() )
  {
    QList<QByteArray> rawHeaders = req.rawHeaderList();
    for (int i = 0; i < rawHeaders.count(); i++)
      PRINT_DEBUG2("SENDING: " << rawHeaders[i] << ":" << req.rawHeader(rawHeaders[i]));

    PRINT_DEBUG2(data);
    PRINT_DEBUG2("decoded again from wbxml; this is what the server sees");
    QByteArray test;
    dataFromWbXml(wbxml, test);
    PRINT_DEBUG2(test);
  }

  QNetworkReply *reply = nam->post(req, wbxml);

  connect(reply, SIGNAL(finished()), eventLoop, SLOT(quit()));
  connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), eventLoop, SLOT(quit()));

  PRINT_DEBUG("sent ...");
  eventLoop->exec();

  if ( reply->isRunning() )  // event loop was stopped prematurely
  {
    reply->abort();
    PRINT_DEBUG("aborted");
    delete reply;
    return QByteArray();
  }

  PRINT_DEBUG("received ...");

  QByteArray result = reply->readAll();

  if ( airSync2().isDebugEnabled() )
  {
    QList<QByteArray> rawHeaders = reply->rawHeaderList();
    for (int i = 0; i < rawHeaders.count(); i++)
      PRINT_DEBUG2("RECEIVED:" << rawHeaders[i] << ":" << reply->rawHeader(rawHeaders[i]));

    PRINT_DEBUG2("HTTP result code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
  }

  if ( reply->error() != QNetworkReply::NoError )
  {
    emit errorMessage(reply->errorString());
    errorOccured = true;

    if ( reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 )
      emit authRequired();

    PRINT_DEBUG("ERROR:" << reply->error() << "http status:" <<
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << reply->errorString());
    PRINT_DEBUG(req.url().toString());

    policyKey = 0; // some error - let's try from scratch
    // e.g. the server sends "Retry after sending a PROVISION command"
  }
  else
  {
    emit errorMessage(QString());
    errorOccured = false;

    if ( !result.isEmpty() )
      dataFromWbXml(result, result);

    PRINT_DEBUG("data:" << result);
  }

  delete reply;

  return result;
}

//--------------------------------------------------------------------------------

void Session::sslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
  reply->ignoreSslErrors();  // ignore all errors

  // log errors
  if ( !wroteSslWarning )
  {
    foreach (QSslError error, errors)
      PRINT_DEBUG("SSL Error: " << error.errorString());

    wroteSslWarning = true;  // always write at least the first SSL errors
  }
}

//--------------------------------------------------------------------------------

bool Session::dataToWbXml(const QByteArray &data, QByteArray &result)
{
  WBXMLTree *tree = 0;

  WBXMLError error =
    wbxml_tree_from_xml(const_cast<WB_UTINY*>(reinterpret_cast<const WB_UTINY*>(data.constData())),
                        data.length(), &tree);

  if ( error != WBXML_OK )
  {
    PRINT_DEBUG("-error- wbxml_tree_from_xml:" << QByteArray::number(error) <<
                QByteArray(reinterpret_cast<const char*>(wbxml_errors_string(error))));
    return false;
  }

  WB_UTINY *wbxml = 0;
  WB_ULONG wbxml_len = 0;
  WBXMLGenWBXMLParams params;
  params.wbxml_version = WBXML_VERSION_13;
  params.keep_ignorable_ws = 0;
  params.use_strtbl = 1;
  params.produce_anonymous = 1;  // important!
  error = wbxml_tree_to_wbxml(tree, &wbxml, &wbxml_len, &params);

  wbxml_tree_destroy(tree);

  if ( error != WBXML_OK )
  {
    PRINT_DEBUG("-error- wbxml_tree_to_wbxml:" << QByteArray::number(error) <<
                QByteArray(reinterpret_cast<const char*>(wbxml_errors_string(error))));
    return false;
  }

  result = QByteArray(reinterpret_cast<const char*>(wbxml), wbxml_len);
  free(wbxml);

  return true;
}

//--------------------------------------------------------------------------------

bool Session::dataFromWbXml(const QByteArray &data, QByteArray &result)
{
  WBXMLGenXMLParams params;
  params.gen_type = WBXML_GEN_XML_INDENT;
  params.lang = WBXML_LANG_AIRSYNC;
  params.indent = 2;
  params.keep_ignorable_ws = 0;

  WB_UTINY *xml = 0;
  WB_ULONG len = 0;

  WBXMLError error =
    wbxml_conv_wbxml2xml_withlen(const_cast<WB_UTINY*>(reinterpret_cast<const WB_UTINY*>(data.constData())),
                                 data.length(), &xml, &len, &params);

  if ( error != WBXML_OK )
  {
    PRINT_DEBUG("-error- wbxml_conv_wbxml2xml_withlen:" << QByteArray::number(error) <<
                QByteArray(reinterpret_cast<const char*>(wbxml_errors_string(error))));
    PRINT_DEBUG("input was:" << data.toPercentEncoding());
    return false;
  }

  result = QByteArray(reinterpret_cast<const char*>(xml), len);
  free(xml);

  return true;
}

//--------------------------------------------------------------------------------

int Session::init()
{
  aborted = false;

  if ( (policyKey != 0) && !collectionId.isEmpty() )   // init already done
    return 0;

  emit progress(2);

  QByteArray data =
        "<Provision xmlns=\"Provision:\">"
          "<Policies>"
            "<Policy>"
              "<PolicyType>MS-EAS-Provisioning-WBXML</PolicyType>"
            "</Policy>"
          "</Policies>"
        "</Provision>";

  data = sendCommand("Provision", data);
  if ( aborted ) return 0;
  if ( errorOccured ) return -1;

  QDomDocument doc;
  if ( !doc.setContent(data) )
  {
    qWarning() << "ERROR: could not parse XML data";
    return -1;
  }

  QDomNodeList list = doc.elementsByTagName("PolicyKey");
  if ( list.isEmpty() )
  {
    qWarning() << "ERROR: PolicyKey not found";
    return -1;
  }

  uint policyKey = list.at(0).firstChild().nodeValue().toUInt();

  setPolicyKey(policyKey);
  emit progress(5);

  data = "<Provision xmlns=\"Provision:\">"
          "<Policies>"
            "<Policy>"
              "<PolicyType>MS-EAS-Provisioning-WBXML</PolicyType>\n"
              "<PolicyKey>" + QByteArray::number(policyKey) + "</PolicyKey>\n"
              "<Status xmlns=\"http://synce.org/formats/airsync_wm5/provision\">1</Status>\n"  // xmlns wichtig !!!!!
            "</Policy>"
          "</Policies>"
        "</Provision>";
  data = sendCommand("Provision", data);
  if ( aborted ) return 0;
  if ( errorOccured ) return -1;

  // set the final session policyKey
  if  ( !doc.setContent(data) )
  {
    qWarning() << "ERROR: could not parse XML data";
    return -1;
  }

  list = doc.elementsByTagName("PolicyKey");
  if ( list.isEmpty() )
  {
    qWarning() << "ERROR: PolicyKey not found";
    return -1;
  }

  setPolicyKey(list.at(0).firstChild().nodeValue().toUInt());
  emit progress(10);

  // get list of available folders
  data = "<FolderSync xmlns=\"FolderHierarchy:\">\n"
    "<SyncKey xmlns=\"http://synce.org/formats/airsync_wm5/folderhierarchy\">0</SyncKey>\n"
    "</FolderSync>\n";
  data = sendCommand("FolderSync", data);
  if ( aborted ) return 0;
  if ( errorOccured ) return -1;

  if ( !doc.setContent(data) )
  {
    qWarning() << "ERROR: could not parse XML data";
    return -1;
  }

  list = doc.elementsByTagName("Add");

  const QByteArray folderName("Inbox");

  for (int i = 0; i < list.count(); i++)
  {
    QDomNode node = list.at(i);

    if ( node.firstChildElement("DisplayName").text().toUtf8() == folderName )
    {
      collectionId = node.firstChildElement("ServerId").text().toUtf8();
      break;
    }
  }

  if ( collectionId.isEmpty() )
  {
    PRINT_DEBUG("Folder '" << folderName << "' not found");
    return -1;
  }

  collectionSyncKey.clear();
  return 0;
}

//--------------------------------------------------------------------------------

int Session::fetchMails()
{
  emit progress(0);

  if ( init() != 0 )
    return -1;

  emit progress(15);  // by definition: after init

  QByteArray data;
  QDomDocument doc;

  if ( collectionSyncKey.isEmpty() )
  {
    // getting synckey for inbox mails
    data = "<Sync>"
             "<Collections>"
               "<Collection>"
                 "<SyncKey>0</SyncKey>"
                 "<CollectionId>" + collectionId + "</CollectionId>"
               "</Collection>"
             "</Collections>"
           "</Sync>";

    data = sendCommand("Sync", data);
    if ( aborted ) return 0;
    if ( errorOccured ) return -1;

    doc.setContent(data);

    collectionSyncKey =
       doc.firstChildElement("Sync")
          .firstChildElement("Collections")
          .firstChildElement("Collection")
          .firstChildElement("SyncKey").text().toUtf8();

    PRINT_DEBUG("collectionSyncKey=" << collectionSyncKey);

    emit progress(17);  // by definition: after first syncKey
  }

  // getting mails
  data = "<Sync>"
           "<Collections>"
             "<Collection>"
               "<SyncKey>" + collectionSyncKey + "</SyncKey>"
               "<CollectionId>" + collectionId + "</CollectionId>"
               "<GetChanges/>"
             "</Collection>"
           "</Collections>"
         "</Sync>";

  data = sendCommand("Sync", data);
  if ( aborted ) return 0;
  if ( errorOccured ) return -1;

  if ( data.isEmpty() )  // no mails to download
  {
    PRINT_DEBUG("no new mails to download");

    emit progress(100);
    return 0;
  }

  doc.setContent(data);

  collectionSyncKey =
     doc.firstChildElement("Sync")
        .firstChildElement("Collections")
        .firstChildElement("Collection")
        .firstChildElement("SyncKey").text().toUtf8();

  PRINT_DEBUG("collectionSyncKey=" << collectionSyncKey);

  emit progress(20);

  // fetch each mail
  QDomNodeList list = doc.elementsByTagName("Add");

  PRINT_DEBUG(list.count() << "new mails to download");

  if ( list.count() == 0 )
  {
    emit progress(100);
    return 0;
  }

  int currentProgress = 20; // 20 - 90% ... download; 90-100% delete
  const int progressPerMail = 70 /* remaining percent */ / list.count();

  for (int i = 0; i < list.count(); i++)
  {
    QDomNode node = list.at(i);
    QByteArray mailId = node.firstChildElement("ServerId").text().toUtf8();

    data = "<ItemOperations"
                " xmlns:airsync='AirSync'"
                " xmlns:airsyncbase='http://synce.org/formats/airsync_wm5/airsyncbase'"
                " xmlns='ItemOperations'>"
             "<Fetch xmlns='http://synce.org/formats/airsync_wm5/itemoperations'>"
               "<Store>Mailbox</Store>"
               "<airsync:CollectionId>" + collectionId + "</airsync:CollectionId>"
               "<airsync:ServerId>" + mailId + "</airsync:ServerId>"
               "<Options>"
                 "<airsync:MIMESupport>2</airsync:MIMESupport>"
                 "<airsyncbase:BodyPreference>"
                   "<airsyncbase:Type>4</airsyncbase:Type>"
                 "</airsyncbase:BodyPreference>"
               "</Options>"
             "</Fetch>"
           "</ItemOperations>";

    data = sendCommand("ItemOperations", data);
    if ( aborted ) return 0;
    if ( errorOccured ) return -1;

    currentProgress += progressPerMail;
    emit progress(currentProgress);

    QDomDocument mailDoc;
    if ( mailDoc.setContent(data) )
    {
      QString fullMail =
        mailDoc.firstChildElement("ItemOperations")
               .firstChildElement("Response")
               .firstChildElement("Fetch")
               .firstChildElement("Properties")
               .firstChildElement("Body")
               .firstChildElement("Data").text();

      if ( fullMail.isEmpty() )
      {
        PRINT_DEBUG("mail is empty (mailId=" << mailId << ") There's probably something going wrong!!");
        continue;
      }

      // handle fullMail
      KMime::Message::Ptr msg(new KMime::Message);
      msg->setContent(KMime::CRLFtoLF(fullMail.toUtf8()));
      msg->parse();
      emit newMessage(msg, mailId);
    }
  }
  emit progress(90);
  return list.count();
}

//--------------------------------------------------------------------------------

int Session::deleteMails(const QList<QByteArray> &mailIds)
{
  QByteArray data;

  data = "<Sync>"
           "<Collections>"
             "<Collection>"
               "<SyncKey>" + collectionSyncKey + "</SyncKey>"
               "<CollectionId>" + collectionId + "</CollectionId>"
               "<DeletesAsMoves>0</DeletesAsMoves>"
               "<Commands>";

  foreach (const QByteArray &mailId, mailIds)
  {
    data +=      "<Delete>"
                   "<ServerId>" + mailId + "</ServerId>"
                 "</Delete>";
  }


  data +=      "</Commands>"
             "</Collection>"
           "</Collections>"
         "</Sync>";

  data = sendCommand("Sync", data);
  if ( errorOccured ) return -1;

  emit progress(90);

  QDomDocument doc;
  if ( !doc.setContent(data) )
  {
    qWarning() << "ERROR: could not parse XML data";
    return -1;
  }

  collectionSyncKey =
     doc.firstChildElement("Sync")
        .firstChildElement("Collections")
        .firstChildElement("Collection")
        .firstChildElement("SyncKey").text().toUtf8();

  PRINT_DEBUG("collectionSyncKey=" << collectionSyncKey);
  PRINT_DEBUG(mailIds.count() << "mails deleted from server");

  emit progress(100);
  return 0;
}

//--------------------------------------------------------------------------------

void Session::abortFetching()
{
  eventLoop->quit();
  aborted = true;
}

//--------------------------------------------------------------------------------
