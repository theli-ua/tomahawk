#include "sourcetreeitem.h"

#include <QDebug>
#include <QTreeView>

#include "tomahawk/collection.h"
#include "tomahawk/playlist.h"
#include "tomahawk/tomahawkapp.h"
#include "sourcesmodel.h"

using namespace Tomahawk;

static inline QList< playlist_ptr > dynListToPlaylists( const QList< Tomahawk::dynplaylist_ptr >& list )
{
    QList< playlist_ptr > newptrs;
    foreach( const dynplaylist_ptr& pl, list ) {
        newptrs << pl.staticCast<Playlist>();
    }
    return newptrs;
}

SourceTreeItem::SourceTreeItem( const source_ptr& source, QObject* parent )
    : QObject( parent )
    , m_source( source )
{
    QStandardItem* item = new QStandardItem( "" );
    item->setEditable( false );
    item->setData( SourcesModel::CollectionSource, Type );
    item->setData( (qlonglong)this, SourceItemPointer );
    m_columns << item;

    if ( !source.isNull() )
    {
        onPlaylistsAdded( source->collection()->playlists() );
        onDynamicPlaylistsAdded( source->collection()->dynamicPlaylists() );

        connect( source->collection().data(), SIGNAL( playlistsAdded( QList<Tomahawk::playlist_ptr> ) ),
                                                SLOT( onPlaylistsAdded( QList<Tomahawk::playlist_ptr> ) ) );

        connect( source->collection().data(), SIGNAL( playlistsDeleted( QList<Tomahawk::playlist_ptr> ) ),
                                                SLOT( onPlaylistsDeleted( QList<Tomahawk::playlist_ptr> ) ) );
        
        connect( source->collection().data(), SIGNAL( dynamicPlaylistsAdded( QList<Tomahawk::dynplaylist_ptr> ) ),
                                                SLOT( onDynamicPlaylistsAdded( QList<Tomahawk::dynplaylist_ptr> ) ) );
        connect( source->collection().data(), SIGNAL( dynamicPlaylistsDeleted( QList<Tomahawk::dynplaylist_ptr> ) ),
                                                SLOT( onDynamicPlaylistsDeleted( QList<Tomahawk::dynplaylist_ptr> ) ) );
    }

    m_widget = new SourceTreeItemWidget( source );
    connect( m_widget, SIGNAL( clicked() ), SLOT( onClicked() ) );
}


SourceTreeItem::~SourceTreeItem()
{
    qDebug() << Q_FUNC_INFO;
}


void
SourceTreeItem::onClicked()
{
    emit clicked( m_columns.at( 0 )->index() );
}


void
SourceTreeItem::onOnline()
{
    m_widget->onOnline();
}


void
SourceTreeItem::onOffline()
{
    m_widget->onOffline();
}


void
SourceTreeItem::onPlaylistsAdded( const QList<playlist_ptr>& playlists )
{
    playlistsAdded( playlists, false );
}

void
SourceTreeItem::onPlaylistsDeleted( const QList<playlist_ptr>& playlists )
{
    playlistsDeleted( playlists, false );
}

void
SourceTreeItem::onPlaylistLoaded( Tomahawk::PlaylistRevision revision )
{
    playlistLoaded( revision, false );
}

void SourceTreeItem::onDynamicPlaylistsAdded( const QList< dynplaylist_ptr >& playlists )
{
    playlistsAdded( dynListToPlaylists( playlists ), true );
}

void SourceTreeItem::onDynamicPlaylistsDeleted( const QList< dynplaylist_ptr >& playlists )
{
    playlistsDeleted( dynListToPlaylists( playlists ), true );
}

void SourceTreeItem::onDynamicPlaylistsLoaded( DynamicPlaylistRevision revision )
{
    playlistLoaded( revision, true );
}

void 
SourceTreeItem::playlistLoaded( PlaylistRevision revision, bool dynamic )
{
    qlonglong ptr = reinterpret_cast<qlonglong>( sender() );
    //qDebug() << "sender ptr:" << ptr;
    
    QStandardItem* item = m_columns.at( 0 );
    int rows = item->rowCount();
    for ( int i = 0; i < rows; i++ )
    {
        QStandardItem* pi = item->child( i );
        qlonglong piptr = pi->data( PlaylistPointer ).toLongLong();
        playlist_ptr* pl = reinterpret_cast<playlist_ptr*>(piptr);
        SourcesModel::SourceType type = static_cast<SourcesModel::SourceType>( pi->data( Type ).toInt() );
        
        if ( ( type == SourcesModel::PlaylistSource || type == SourcesModel::DynamicPlaylistSource ) && ptr == qlonglong( pl->data() ) )
        {
            //qDebug() << "Found playlist!";
            pi->setEnabled( true );
            if( dynamic )
                m_current_dynamic_revisions.insert( pl->data()->guid(), revision.revisionguid );
            else
                m_current_revisions.insert( pl->data()->guid(), revision.revisionguid );
        }
    }
}

void 
SourceTreeItem::playlistsAdded( const QList< playlist_ptr >& playlists, bool dynamic )
{
    // const-ness is important for getting the right pointer!
    foreach( const playlist_ptr& p, playlists )
    {
        qlonglong ptr;
        if( dynamic ) 
        {
            m_dynplaylists.append( p.staticCast<Tomahawk::DynamicPlaylist>() );
            ptr = reinterpret_cast<qlonglong>( &m_dynplaylists.last() );
            
            connect( p.staticCast<Tomahawk::DynamicPlaylist>().data(), SIGNAL( revisionLoaded( Tomahawk::DynamicPlaylistRevision ) ),
                     SLOT( onDynamicPlaylistLoaded( Tomahawk::DynamicPlaylistRevision ) ),
                     Qt::QueuedConnection);
        } else
        {
            m_playlists.append( p );
            ptr = reinterpret_cast<qlonglong>( &m_playlists.last() );
            
            connect( p.data(), SIGNAL( revisionLoaded( Tomahawk::PlaylistRevision ) ),
                     SLOT( onPlaylistLoaded( Tomahawk::PlaylistRevision ) ),
                     Qt::QueuedConnection);
        }
        qDebug() << "Playlist added:" << p->title() << p->creator() << p->info() << ptr << dynamic;
        
        
        QStandardItem* subitem = new QStandardItem( p->title() );
        subitem->setIcon( QIcon( RESPATH "images/playlist-icon.png" ) );
        subitem->setEditable( false );
        subitem->setEnabled( false );
        subitem->setData( ptr, PlaylistPointer );
        subitem->setData( dynamic ? SourcesModel::DynamicPlaylistSource : SourcesModel::DynamicPlaylistSource, Type );
        subitem->setData( (qlonglong)this, SourceItemPointer );
        
        m_columns.at( 0 )->appendRow( subitem );
        Q_ASSERT( qobject_cast<QTreeView*>((parent()->parent()) ) );
        qobject_cast<QTreeView*>((parent()->parent()))->expandAll();
        
        p->loadRevision();
    }
}

void 
SourceTreeItem::playlistsDeleted( const QList< playlist_ptr >& playlists, bool dynamic )
{
    // const-ness is important for getting the right pointer!
    foreach( const playlist_ptr& p, playlists )
    {
        qlonglong ptr = qlonglong( p.data() );
        qDebug() << "Playlist removed:" << p->title() << p->creator() << p->info() << ptr << dynamic;
        
        QStandardItem* item = m_columns.at( 0 );
        int rows = item->rowCount();
        for ( int i = rows - 1; i >= 0; i-- )
        {
            QStandardItem* pi = item->child( i );
            qlonglong piptr = pi->data( PlaylistPointer ).toLongLong();
            playlist_ptr* pl = reinterpret_cast<playlist_ptr*>(piptr);
            SourcesModel::SourceType type = static_cast<SourcesModel::SourceType>( pi->data( Type ).toInt() );
            
            if ( ( type == SourcesModel::PlaylistSource || type == SourcesModel::DynamicPlaylistSource ) && ptr == qlonglong( pl->data() ) )
            {
                if( dynamic )
                    m_dynplaylists.removeAll( p.staticCast<Tomahawk::DynamicPlaylist>() );
                else
                    m_playlists.removeAll( p );
                item->removeRow( i );
            }
        }
    }
}
