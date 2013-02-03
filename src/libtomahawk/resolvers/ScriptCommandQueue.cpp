/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2013, Teo Mrnjavac <teo@kde.org>
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

#include "ScriptCommandQueue.h"

#include "utils/Closure.h"

#include <QMetaType>

ScriptCommandQueue::ScriptCommandQueue( QObject* parent )
    : QObject( parent )
    , m_timer( new QTimer( this ) )
{
    m_timer->setSingleShot( true );
}


void
ScriptCommandQueue::enqueue( const QSharedPointer< ScriptCommand >& req )
{
    m_queue.append( req );
    if ( m_queue.count() == 1 )
        nextCommand();
}


void
ScriptCommandQueue::nextCommand()
{
    if ( m_queue.isEmpty() )
        return;

    QSharedPointer< ScriptCommand > req = m_queue.first();

    NewClosure( req.data(), SIGNAL( done() ),
                this, SLOT( onCommandDone( QSharedPointer< ScriptCommand > ) ), req );

    NewClosure( m_timer, SIGNAL( timeout() ),
                this, SLOT( onTimeout( QSharedPointer< ScriptCommand > ) ), req );

    m_timer->start( 5000 );

    req->exec();
}


void
ScriptCommandQueue::onCommandDone( const QSharedPointer< ScriptCommand >& req )
{
    disconnect( this, SLOT( onTimeout( QSharedPointer< ScriptCommand > ) ) );
    m_timer->stop();

    m_queue.removeAll( req );

    if ( m_queue.count() > 0 )
        nextCommand();
}


void
ScriptCommandQueue::onTimeout( const QSharedPointer< ScriptCommand >& req )
{
    disconnect( this, SLOT( onCommandDone( QSharedPointer< ScriptCommand > ) ) );

    m_timer->stop();

    req->reportFailure();
    m_queue.removeAll( req );

    if ( m_queue.count() > 0 )
        nextCommand();
}
