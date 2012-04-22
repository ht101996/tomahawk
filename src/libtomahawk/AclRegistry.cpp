/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2010-2012, Jeff Mitchell <jeff@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AclRegistry.h"

#include <QThread>
#include <QVariant>

#include "TomahawkSettings.h"
#include "TomahawkApp.h"

#include "utils/Logger.h"
#include "jobview/AclJobItem.h"
#include "jobview/JobStatusView.h"
#include "jobview/JobStatusModel.h"


QDataStream& operator<<( QDataStream &out, const ACLRegistry::User &user )
{
    out << ACLUSERVERSION;
    out << user.uuid;
    out << user.knownDbids.length();
    foreach( QString knownDbid, user.knownDbids )
        out << knownDbid;
    out << user.knownAccountIds.length();
    foreach( QString knownAccount, user.knownAccountIds )
        out << user.knownAccountIds;
    out << (int)( user.acl );
    return out;
}

QDataStream& operator>>( QDataStream &in, ACLRegistry::User &user )
{
    int ver;
    in >> ver;
    if ( ver == ACLUSERVERSION )
    {
        in >> user.uuid;
        int dbidsLength;
        in >> dbidsLength;
        for ( int i = 0; i < dbidsLength; i++ )
            in >> user.knownDbids;
        int accountsLength;
        in >> accountsLength;
        for ( int i = 0; i < accountsLength; i++ )
            in >> user.knownAccountIds;
        int aclIn;
        in >> aclIn;
        user.acl = (ACLRegistry::ACL)( aclIn );
    }
    return in;
}


ACLRegistry* ACLRegistry::s_instance = 0;

ACLRegistry*
ACLRegistry::instance()
{
    if( !s_instance )
        new ACLRegistry();
    return s_instance;
}


ACLRegistry::ACLRegistry( QObject* parent )
    : QObject( parent )
    , m_jobCount( 0 )
{
    s_instance = this;
    qRegisterMetaType< ACLRegistry::ACL >( "ACLRegistry::ACL" );
    qRegisterMetaType< ACLRegistry::User >( "ACLRegistry::User" );
    qRegisterMetaTypeStreamOperators< ACLRegistry::User >( "ACLRegistry::User" );
    load();
}


ACLRegistry::~ACLRegistry()
{
}


ACLRegistry::ACL
ACLRegistry::isAuthorizedUser( const QString& dbid, const QString &username, ACLRegistry::ACL globalType, bool skipEmission )
{
    tDebug() << Q_FUNC_INFO;
    if ( QThread::currentThread() != TOMAHAWK_APPLICATION::instance()->thread() )
    {
        if ( !skipEmission )
            QMetaObject::invokeMethod( this, "isAuthorizedUser", Qt::QueuedConnection, Q_ARG( const QString&, dbid ), Q_ARG( const QString &, username ), Q_ARG( ACLRegistry::ACL, globalType ), Q_ARG( bool, skipEmission ) );
        return ACLRegistry::NotFound;
    }
    tDebug() << Q_FUNC_INFO << "in right thread";
    //FIXME: Remove when things are working
//     emit aclResult( dbid, username, ACLRegistry::Stream );
//     return ACLRegistry::NotFound;
    
    bool found = false;
    QMutableListIterator< ACLRegistry::User > i( m_cache );
    while ( i.hasNext() )
    {
        ACLRegistry::User user = i.next();
        foreach ( QString knowndbid, user.knownDbids )
        {
            if ( dbid == knowndbid )
            {
                if ( !user.knownAccountIds.contains( username ) )
                    user.knownAccountIds.append( username );
                found = true;
            }
        }
        
        foreach ( QString knownaccountid, user.knownAccountIds )
        {
            if ( username == knownaccountid )
            {
                if ( !user.knownDbids.contains( dbid ) )
                    user.knownDbids.append( dbid );
                found = true;
            }
        }

        if ( found )
        {
            if ( !skipEmission )
                emit aclResult( dbid, username, user.acl );
            i.setValue( user );
            return user.acl;
        }
    }

    if ( skipEmission )
        return ACLRegistry::NotFound;
    
    // User was not found, create a new user entry
    ACLRegistry::User user;
    user.knownDbids.append( dbid );
    user.knownAccountIds.append( username );
    if ( globalType != ACLRegistry::NotFound )
        user.acl = globalType;
#ifndef ENABLE_HEADLESS
    else
    {
        getUserDecision( user, username );
        return ACLRegistry::NotFound;
    }
#endif
    m_cache.append( user );
    emit aclResult( dbid, username, user.acl );
    return user.acl;
}


#ifndef ENABLE_HEADLESS
void
ACLRegistry::getUserDecision( ACLRegistry::User user, const QString &username )
{
    tDebug() << Q_FUNC_INFO;
    AclJobItem* job = new AclJobItem( user, username );
    m_jobQueue.enqueue( job );
    queueNextJob();
}
#endif


void
ACLRegistry::userDecision( ACLRegistry::User user )
{
    tDebug() << Q_FUNC_INFO;
    m_cache.append( user );
    save();
    emit aclResult( user.knownDbids.first(), user.knownAccountIds.first(), user.acl );

    m_jobCount--;
    if ( !m_jobQueue.isEmpty() )
        queueNextJob();
}


void
ACLRegistry::queueNextJob()
{
    tDebug() << Q_FUNC_INFO << "jobCount = " << m_jobCount;
    if ( m_jobCount != 0 )
        return;

    tDebug() << Q_FUNC_INFO << "jobQueue size = " << m_jobQueue.length();
    if ( !m_jobQueue.isEmpty() )
    {
        AclJobItem* job = m_jobQueue.dequeue();
        ACLRegistry::User user = job->user();
        bool found = false;
        foreach( QString dbid, user.knownDbids )
        {
            ACLRegistry::ACL acl = isAuthorizedUser( dbid, job->username(), ACLRegistry::NotFound, true );
            if ( acl != ACLRegistry::NotFound )
            {
                tDebug() << Q_FUNC_INFO << "Found existing acl entry for = " << user.knownAccountIds.first();
                found = true;
                break;
            }
        }
        if ( found )
        {
            tDebug() << Q_FUNC_INFO << "deleting job, already have ACL for " << user.knownAccountIds.first();
            delete job;
            QTimer::singleShot( 0, this, SLOT( queueNextJob() ) );
            return;
        }
        else
        {
            tDebug() << Q_FUNC_INFO << "activating job for user" << user.knownAccountIds.first();
            m_jobCount++;
            JobStatusView::instance()->model()->addJob( job );
            connect( job, SIGNAL( userDecision( ACLRegistry::User ) ), this, SLOT( userDecision( ACLRegistry::User ) ) );
        }
    }
}


void
ACLRegistry::load()
{
    tDebug() << Q_FUNC_INFO;
    QVariantList entryList = TomahawkSettings::instance()->aclEntries();
    foreach ( QVariant entry, entryList )
    {
        if ( !entry.isValid() || !entry.canConvert< ACLRegistry::User >() )
            continue;
        tDebug() << Q_FUNC_INFO << "loading entry";
        ACLRegistry::User entryUser = entry.value< ACLRegistry::User >();
        m_cache.append( entryUser );
    }
}


void
ACLRegistry::save()
{
    tDebug() << Q_FUNC_INFO;
    QVariantList entryList;
    foreach ( ACLRegistry::User user, m_cache )
    {
        tDebug() << Q_FUNC_INFO << "user is " << user.uuid << " with known name " << user.knownAccountIds.first();
        entryList.append( QVariant::fromValue< ACLRegistry::User >( user ) );
    }
    TomahawkSettings::instance()->setAclEntries( entryList );
}