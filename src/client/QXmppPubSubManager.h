/*
 * Copyright (C) 2008-2020 The QXmpp developers
 *
 * Author:
 *  Germán Márquez Mejía
 *
 * Source:
 *  https://github.com/qxmpp-project/qxmpp
 *
 * This file is a part of QXmpp library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef QXMPPPUBSUBMANAGER_H
#define QXMPPPUBSUBMANAGER_H

#include "QXmppClient.h"
#include "QXmppClientExtension.h"
#include "QXmppMessage.h"
#include "QXmppPubSubIq.h"

#include <QFuture>
#include <QFutureWatcher>

class QXmppPubSubPublishOptions;

class QXMPP_EXPORT QXmppPubSubEventManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    virtual bool handlePubSubEvent(const QDomElement &element, const QString &pubSubService, const QString &nodeName) = 0;
};

class QXMPP_EXPORT QXmppPubSubManager : public QXmppClientExtension
{
    Q_OBJECT

public:
    using Success = std::monostate;
    using Result = std::variant<Success, QXmppStanza::Error, QXmpp::PacketState>;
    using InstantNodeResult = std::variant<QString, QXmppStanza::Error, QXmpp::PacketState>;
    template<typename T>
    using ItemResult = std::variant<std::optional<T>, QXmppStanza::Error, QXmpp::PacketState>;
    template<typename T>
    using ItemsResult = std::variant<QList<T>, QXmppStanza::Error, QXmpp::PacketState>;
    using PublishItemResult = std::variant<QString, QXmppStanza::Error, QXmpp::PacketState>;
    using PublishItemsResult = std::variant<QVector<QString>, QXmppStanza::Error, QXmpp::PacketState>;

    QXmppPubSubManager();
    ~QXmppPubSubManager();

    // Generic PubSub (the PubSub service is the given entity)
    QFuture<Result> createNode(const QString &jid, const QString &nodeName);
    QFuture<InstantNodeResult> createInstantNode(const QString &jid);
    QFuture<Result> deleteNode(const QString &jid, const QString &nodeName);
    template<typename T>
    QFuture<ItemResult<T>> requestItem(const QString &jid, const QString &nodeName, const QString &itemId);
    template<typename T>
    QFuture<ItemsResult<T>> requestItems(const QString &jid, const QString &nodeName);
    template<typename T>
    QFuture<ItemsResult<T>> requestItems(const QString &jid, const QString &nodeName, const QStringList &itemIds);
    template<typename T>
    QFuture<PublishItemResult> publishItem(const QString &jid, const QString &nodeName, const T &items);
    template<typename T>
    QFuture<PublishItemResult> publishItem(const QString &jid, const QString &nodeName, const T &items, const QXmppPubSubPublishOptions &publishOptions);
    template<typename T>
    QFuture<PublishItemsResult> publishItems(const QString &jid, const QString &nodeName, const QList<T> &items);
    template<typename T>
    QFuture<PublishItemsResult> publishItems(const QString &jid, const QString &nodeName, const QList<T> &items, const QXmppPubSubPublishOptions &publishOptions);
    QFuture<Result> retractItem(const QString &jid, const QString &nodeName, const QString &itemId);
    QFuture<Result> purgeItems(const QString &jid, const QString &nodeName);

    // PEP-specific (the PubSub service is the current account)
    QFuture<Result> createPepNode(const QString &nodeName);
    QFuture<Result> deletePepNode(const QString &nodeName);
    template<typename T>
    QFuture<PublishItemResult> publishPepItem(const QString &nodeName, const T &item, const QXmppPubSubPublishOptions &publishOptions);
    template<typename T>
    QFuture<PublishItemResult> publishPepItem(const QString &nodeName, const T &item);
    template<typename T>
    QFuture<PublishItemsResult> publishPepItems(const QString &nodeName, const QList<T> &items, const QXmppPubSubPublishOptions &publishOptions);
    template<typename T>
    QFuture<PublishItemsResult> publishPepItems(const QString &nodeName, const QList<T> &items);
    QFuture<Result> retractPepItem(const QString &nodeName, const QString &itemId);
    QFuture<Result> purgePepItems(const QString &nodeName);

    /// \cond
    bool handleStanza(const QDomElement &element) override;
    /// \endcond

private:
    /// \cond
    QFuture<Result> sendGenericIq(const QXmppIq &iq);
    QFuture<PublishItemResult> publishItem(QXmppPubSubIqBase &&iq);
    QFuture<PublishItemsResult> publishItems(QXmppPubSubIqBase &&iq);
    static QXmppPubSubIq<> requestItemsIq(const QString &jid, const QString &nodeName, const QStringList &itemIds);
    /// \endcond

    // We may need a d-ptr in the future.
    void *d;
};

///
/// Requests a specific item of an entity's node.
///
/// \param jid Jabber ID of the entity hosting the pubsub service. For PEP this
/// should be an account's bare JID
/// \param nodeName the name of the node to query
/// \param itemId the ID of the item to retrieve
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::ItemResult<T>> QXmppPubSubManager::requestItem(const QString &jid,
                                                                           const QString &nodeName,
                                                                           const QString &itemId)
{
    QFutureInterface<ItemResult<T>> resultInterface(QFutureInterfaceBase::Started);

    auto sendFuture = client()->sendIq(requestItemsIq(jid, nodeName, {itemId}));

    auto *watcher = new QFutureWatcher<QXmppClient::IqResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [=](){
        auto result = watcher->result();
        if (auto element = std::get_if<QDomElement>(&result)) {
            if (QXmppPubSubIq<T>::isPubSubIq(*element)) {
                // FIXME: could this still be an IQ of type error?
                QXmppPubSubIq<T> resultIq;
                resultIq.parse(*element);

                if (resultIq.items().isEmpty()) {
                    resultInterface.reportResult(std::nullopt);
                } else {
                    resultInterface.reportResult(resultIq.items().at(0));
                }
            } else {
                QXmppIq iq;
                iq.parse(*element);
                resultInterface.reportResult(iq.error());
            }
        } else if (const auto *error = std::get_if<QXmpp::PacketState>(&result)) {
            resultInterface.reportResult(*error);
        }
        resultInterface.reportFinished();
        watcher->deleteLater();
    });
    watcher->setFuture(sendFuture);

    return resultInterface.future();
}

///
/// Requests all items of an entity's node.
///
/// \param jid Jabber ID of the entity hosting the pubsub service. For PEP this
/// should be an account's bare JID
/// \param nodeName the name of the node to query
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::ItemsResult<T>> QXmppPubSubManager::requestItems(const QString &jid,
                                                                             const QString &nodeName)
{
    return requestItems<T>(jid, nodeName, {});
}

///
/// Requests items of an entity's node.
///
/// \param jid Jabber ID of the entity hosting the pubsub service. For PEP this
/// should be an account's bare JID
/// \param nodeName the name of the node to query
/// \param itemIds the IDs of the items to retrieve. If empty, retrieves all
/// items
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::ItemsResult<T>> QXmppPubSubManager::requestItems(const QString &jid,
                                                                             const QString &nodeName,
                                                                             const QStringList &itemIds)
{
    QFutureInterface<ItemsResult<T>> resultInterface(QFutureInterfaceBase::Started);

    auto sendFuture = client()->sendIq(requestItemsIq(jid, nodeName, itemIds));

    auto *watcher = new QFutureWatcher<QXmppClient::IqResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [=](){
        auto result = watcher->result();
        if (auto element = std::get_if<QDomElement>(&result)) {
            if (QXmppPubSubIq<T>::isPubSubIq(*element)) {
                // FIXME: could this still be an IQ of type error?
                QXmppPubSubIq<T> resultIq;
                resultIq.parse(*element);
                resultInterface.reportResult(resultIq.items());
                resultInterface.reportFinished();
            } else {
                QXmppIq iq;
                iq.parse(*element);

                resultInterface.reportResult(iq.error());
            }
        } else if (const auto *error = std::get_if<QXmpp::PacketState>(&result)) {
            resultInterface.reportResult(*error);
            resultInterface.reportFinished();
        }
        watcher->deleteLater();
    });
    watcher->setFuture(sendFuture);

    return resultInterface.future();
}

///
/// Publishs one item to a pubsub node.
///
/// This is a convenience method equivalent to calling
/// QXmppPubSubManager::publishItem with no publish options.
///
/// \param jid Jabber ID of the entity hosting the pubsub service
/// \param nodeName the name of the node to publish the item to
/// \param item the item to publish
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemResult> QXmppPubSubManager::publishItem(const QString &jid,
                                                                               const QString &nodeName,
                                                                               const T &item)
{
    QXmppPubSubIq<T> request;
    request.setTo(jid);
    request.setItems({item});
    request.setQueryNode(nodeName);
    return publishItem(std::move(request));
}

///
/// Publishs one item to a pubsub node.
///
/// This is a convenience method equivalent to calling
/// QXmppPubSubManager::publishItem with no publish options.
///
/// \param jid Jabber ID of the entity hosting the pubsub service
/// \param nodeName the name of the node to publish the item to
/// \param item the item to publish
/// \param publishOptions publish-options for the items
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemResult> QXmppPubSubManager::publishItem(const QString &jid,
                                                                               const QString &nodeName,
                                                                               const T &item,
                                                                               const QXmppPubSubPublishOptions &publishOptions)
{
    QXmppPubSubIq<T> request;
    request.setTo(jid);
    request.setItems({item});
    request.setQueryNode(nodeName);
    request.setPublishOptions(publishOptions);
    return publishItem(std::move(request));
}

///
/// Publishs items to a pubsub node.
///
/// \param jid Jabber ID of the entity hosting the pubsub service
/// \param nodeName the name of the node to publish the items to
/// \param items the items to publish
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemsResult> QXmppPubSubManager::publishItems(const QString &jid,
                                                                                 const QString &nodeName,
                                                                                 const QList<T> &items)
{
    QXmppPubSubIq<T> request;
    request.setTo(jid);
    request.setItems(items);
    request.setQueryNode(nodeName);
    return publishItems(std::move(request));
}

///
/// Publishs items to a pubsub node.
///
/// \param jid Jabber ID of the entity hosting the pubsub service
/// \param nodeName the name of the node to publish the items to
/// \param items the items to publish
/// \param publishOptions publish-options for the items
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemsResult> QXmppPubSubManager::publishItems(const QString &jid,
                                                                                 const QString &nodeName,
                                                                                 const QList<T> &items,
                                                                                 const QXmppPubSubPublishOptions &publishOptions)
{
    QXmppPubSubIq<T> request;
    request.setTo(jid);
    request.setItems(items);
    request.setQueryNode(nodeName);
    request.setPublishOptions(publishOptions);
    return publishItems(std::move(request));
}

///
/// Publishs one item to a PEP node.
///
/// \param nodeName the name of the PEP node to publish the item to
/// \param item the item to publish
/// \param publishOptions publish-options for fine tuning
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemResult> QXmppPubSubManager::publishPepItem(const QString &nodeName, const T &item, const QXmppPubSubPublishOptions &publishOptions)
{
    return publishItem(client()->configuration().jidBare(), nodeName, item, publishOptions);
}

///
/// Publishs one item to a PEP node.
///
/// \param nodeName the name of the PEP node to publish the item to
/// \param item the item to publish
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemResult> QXmppPubSubManager::publishPepItem(const QString &nodeName, const T &item)
{
    return publishItem(client()->configuration().jidBare(), nodeName, item);
}

///
/// Publishs items to a PEP node.
///
/// \param nodeName the name  of the PEP node to publish the items to
/// \param items the items to publish
/// \param publishOptions publish-options for fine tuning (optional). Pass
/// an empty form to honor the default options of the PEP node
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemsResult> QXmppPubSubManager::publishPepItems(const QString &nodeName, const QList<T> &items, const QXmppPubSubPublishOptions &publishOptions)
{
    return publishItems(client()->configuration().jidBare(), nodeName, items, publishOptions);
}

///
/// Publishs items to a PEP node.
///
/// \param nodeName the name of the PEP node to publish the items to
/// \param items the items to publish
/// \return
///
template<typename T>
QFuture<QXmppPubSubManager::PublishItemsResult> QXmppPubSubManager::publishPepItems(const QString &nodeName, const QList<T> &items)
{
    return publishItems(client()->configuration().jidBare(), nodeName, items);
}

#endif // QXMPPPUBSUBMANAGER_H
