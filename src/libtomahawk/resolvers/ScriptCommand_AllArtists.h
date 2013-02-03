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

#ifndef SCRIPTCOMMAND_ALLARTISTS_H
#define SCRIPTCOMMAND_ALLARTISTS_H

#include "collection/ArtistsRequest.h"
#include "collection/Collection.h"
#include "resolvers/ScriptCommand.h"

class ScriptCommand_AllArtists : public ScriptCommand
                               , public Tomahawk::ArtistsRequest
{
    Q_OBJECT
public:
    explicit ScriptCommand_AllArtists( const Tomahawk::collection_ptr& collection,
                                       QObject* parent = 0 );
    virtual ~ScriptCommand_AllArtists() {}

    virtual void enqueue();
    
signals:
    void artists( const QList< Tomahawk::artist_ptr >& );
    void done();

protected:
    virtual void exec();
    virtual void reportFailure();

private slots:
    void onResolverDone( const QList< Tomahawk::artist_ptr >& );

private:
    Tomahawk::collection_ptr m_collection;
};

#endif // SCRIPTCOMMAND_ALLARTISTS_H
