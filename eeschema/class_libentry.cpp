/**********************************************************/
/*  lib_entry.cpp                                         */
/**********************************************************/

#include "fctsys.h"
#include "common.h"
#include "kicad_string.h"
#include "confirm.h"
#include "class_drawpanel.h"
#include "gr_basic.h"

#include "program.h"
#include "libcmp.h"
#include "general.h"
#include "protos.h"


int SortItemsFct(const void* ref, const void* item)
{
#define Ref    ( *(LibEDA_BaseStruct**) (ref) )
#define Item   ( *(LibEDA_BaseStruct**) (item) )
#define BEFORE -1
#define AFTER  1

    int fill_ref = 0, fill_item = 0;

    switch( Ref->Type() )
    {
    case COMPONENT_ARC_DRAW_TYPE:
    {
        const LibDrawArc* draw = (const LibDrawArc*) Ref;
        fill_ref = draw->m_Fill;
        break;
    }

    case COMPONENT_CIRCLE_DRAW_TYPE:
    {
        const LibDrawCircle* draw = (const LibDrawCircle*) Ref;
        fill_ref = draw->m_Fill;
        break;
    }

    case COMPONENT_RECT_DRAW_TYPE:
    {
        const LibDrawSquare* draw = (const LibDrawSquare*) Ref;
        fill_ref = draw->m_Fill;
        break;
    }

    case COMPONENT_POLYLINE_DRAW_TYPE:
    {
        const LibDrawPolyline* draw = (const LibDrawPolyline*) Ref;
        fill_ref = draw->m_Fill;
        break;
    }

    case COMPONENT_GRAPHIC_TEXT_DRAW_TYPE:
        if( Item->Type() == COMPONENT_PIN_DRAW_TYPE )
            return BEFORE;
        if( Item->Type() == COMPONENT_GRAPHIC_TEXT_DRAW_TYPE )
            return 0;
        return 1;
        break;

    case COMPONENT_PIN_DRAW_TYPE:
        if( Item->Type() == COMPONENT_PIN_DRAW_TYPE )
        {
            int ii;

            // Sort the pins by orientation
            ii = ( (LibDrawPin*) Ref )->m_Orient -
                ( (LibDrawPin*) Item )->m_Orient;
            if( ii )
                return ii;

            /* We sort the pins by position (x or y).
             * note: at this point, most of pins have same x pos or y pos,
             * because they are sorted by orientation and generally are
             * vertically or horizontally aligned */
            wxPoint pos_ref, pos_tst;
            pos_ref = ( (LibDrawPin*) Ref )->m_Pos;
            pos_tst = ( (LibDrawPin*) Item )->m_Pos;
            if( (ii = pos_ref.x - pos_tst.x) )
                return ii;
            ii = pos_ref.y - pos_tst.y;
            return ii;
        }
        else
            return AFTER;
        break;

    default:
        ;
    }

    /* Test de l'item */
    switch( Item->Type() )
    {
    case COMPONENT_ARC_DRAW_TYPE:
    {
        const LibDrawArc* draw = (const LibDrawArc*) Item;
        fill_item = draw->m_Fill;
        break;
    }

    case COMPONENT_CIRCLE_DRAW_TYPE:
    {
        const LibDrawCircle* draw = (const LibDrawCircle*) Item;
        fill_item = draw->m_Fill;
        break;
    }

    case COMPONENT_RECT_DRAW_TYPE:
    {
        const LibDrawSquare* draw = (const LibDrawSquare*) Item;
        fill_item = draw->m_Fill;
        break;
    }

    case COMPONENT_POLYLINE_DRAW_TYPE:
    {
        const LibDrawPolyline* draw = (const LibDrawPolyline*) Item;
        fill_item = draw->m_Fill;
        break;
    }

    case COMPONENT_GRAPHIC_TEXT_DRAW_TYPE:
        return BEFORE;
        break;

    case COMPONENT_PIN_DRAW_TYPE:
        return BEFORE;
        break;

    default:
        ;
    }

    if( fill_ref & fill_item )
        return 0;
    if( fill_ref )
        return BEFORE;
    return AFTER;
}


/*********************/
/* class CMP_LIB_ENTRY */
/*********************/

/* Basic class for library component description
 *  Not directly used
 *  Used to create the 2 derived classes :
 *      - LIB_ALIAS
 *      - LIB_COMPONENT
 */

/********************************************************************/
CMP_LIB_ENTRY::CMP_LIB_ENTRY( LibrEntryType type, const wxString& name,
                              CMP_LIBRARY* lib ) :
    EDA_BaseStruct( LIBCOMPONENT_STRUCT_TYPE )
{
    Type = type;
    m_Name.m_FieldId = VALUE;
    m_Name.SetParent( this );
    m_Name.m_Text = name;
    m_lib = lib;
}


CMP_LIB_ENTRY::~CMP_LIB_ENTRY()
{
}


wxString CMP_LIB_ENTRY::GetLibraryName()
{
    if( m_lib != NULL )
        return m_lib->GetName();

    return wxEmptyString;
}


bool CMP_LIB_ENTRY::operator==( const wxChar* name ) const
{
    return m_Name.m_Text.CmpNoCase( name ) == 0;
}


bool operator<( const CMP_LIB_ENTRY& item1, const CMP_LIB_ENTRY& item2 )
{
    return item1.m_Name.m_Text.CmpNoCase( item2.m_Name.m_Text ) < 0;
}


int LibraryEntryCompare( const CMP_LIB_ENTRY* LE1, const CMP_LIB_ENTRY* LE2 )
{
    return LE1->m_Name.m_Text.CmpNoCase( LE2->m_Name.m_Text );
}


/*******************************/
/* class LIB_ALIAS */
/*******************************/

/* Class to define an alias of a component
 *  An alias uses the component definition (graphic, pins...)
 *  but has its own name and documentation.
 *  Therefore, when the component is modified, alias of this component are
 *   modified.
 *  This is a simple method to create components with differs very few
 *  (like 74LS00, 74HC00 ... and many op amps )
 */

LIB_ALIAS::LIB_ALIAS( const wxString& name, LIB_COMPONENT* root,
                      CMP_LIBRARY* lib ) :
    CMP_LIB_ENTRY( ALIAS, name, lib )
{
    wxASSERT( root != NULL && root->Type == ROOT );

    m_root = root;
}


LIB_ALIAS::~LIB_ALIAS()
{
}


void LIB_ALIAS::SetComponent( LIB_COMPONENT* root )
{
    wxASSERT( root != NULL && root->Type == ROOT );

    m_root =  root;
}


/********************************/
/* class LIB_COMPONENT */
/********************************/

/* This is a standard component  (in library)
 */
LIB_COMPONENT::LIB_COMPONENT( const wxString& name, CMP_LIBRARY* lib ) :
    CMP_LIB_ENTRY( ROOT, name, lib )
{
    m_Drawings            = NULL;
    m_LastDate            = 0;
    m_UnitCount           = 1;
    m_TextInside          = 40;
    m_Options             = ENTRY_NORMAL;
    m_UnitSelectionLocked = FALSE;
    m_DrawPinNum          = 1;
    m_DrawPinName         = 1;
    m_Prefix.m_FieldId    = REFERENCE;
    m_Prefix.SetParent( this );
}


LIB_COMPONENT::~LIB_COMPONENT()
{
    LibEDA_BaseStruct* DrawItem;
    LibEDA_BaseStruct* NextDrawItem;

    /* suppression des elements dependants */
    DrawItem = m_Drawings;
    m_Drawings = NULL;

    while( DrawItem )
    {
        NextDrawItem = DrawItem->Next();
        SAFE_DELETE( DrawItem );
        DrawItem = NextDrawItem;
    }
}


void LIB_COMPONENT::Draw( WinEDA_DrawPanel* panel, wxDC* dc,
                          const wxPoint& offset, int multi,
                          int convert, int drawMode, int color,
                          const int transformMatrix[2][2],
                          bool showPinText, bool drawFields,
                          bool onlySelected )
{
    wxString           fieldText;
    LibDrawField*      Field;
    LibEDA_BaseStruct* drawItem;
    BASE_SCREEN*       screen = panel->GetScreen();

    GRSetDrawMode( dc, drawMode );

    for( drawItem = m_Drawings; drawItem != NULL; drawItem = drawItem->Next() )
    {
        if( onlySelected && drawItem->m_Selected == 0 )
            continue;

        // Do not draw an item while moving (the cursor handler does that)
        if( drawItem->m_Flags & IS_MOVED )
            continue;

        /* Do not draw items not attached to the current part */
        if( multi && drawItem->m_Unit && ( drawItem->m_Unit != multi ) )
            continue;

        if( convert && drawItem->m_Convert && ( drawItem->m_Convert != convert ) )
            continue;


        if( drawItem->Type() == COMPONENT_PIN_DRAW_TYPE )
        {
            drawItem->Draw( panel, dc, offset, color, drawMode, &showPinText,
                            transformMatrix );
        }
        else
        {
            bool force_nofill =
                ( screen->m_IsPrinting
                  && drawItem->m_Fill == FILLED_WITH_BG_BODYCOLOR
                  && GetGRForceBlackPenState() );

            drawItem->Draw( panel, dc, offset, color, drawMode,
                            (void*) force_nofill, transformMatrix );
        }
    }

    if( drawFields )
    {
        /* The reference designator field is a special case for naming
         * convention.
         *
         * FIXME: This should be handled by the LibDrawField class.
         */
        if( m_UnitCount > 1 )
        {
#if defined(KICAD_GOST)
            fieldText.Printf( wxT( "%s?.%c" ), (const wxChar*) m_Prefix.m_Text,
                              multi + '1' - 1 );
#else
            fieldText.Printf( wxT( "%s?%c" ), (const wxChar*) m_Prefix.m_Text,
                              multi + 'A' - 1 );
#endif
        }
        else
        {
            fieldText = m_Prefix.m_Text + wxT( "?" );
        }

        m_Prefix.Draw( panel, dc, offset, color, drawMode, &fieldText,
                       transformMatrix );
        m_Name.Draw( panel, dc, offset, color, drawMode, NULL, transformMatrix );

        for( Field = m_Fields; Field != NULL; Field = Field->Next() )
        {
            Field->Draw( panel, dc, offset, color, drawMode, NULL,
                         transformMatrix );
        }
    }

    /* Enable this to draw the anchor of the component. */
#if 0
    int len = panel->GetScreen()->Unscale( 3 );
    GRLine( &panel->m_ClipBox, dc, offset.x, offset.y - len, offset.x,
            offset.y + len, 0, color );
    GRLine( &panel->m_ClipBox, dc, offset.x - len, offset.y, offset.x + len,
            offset.y, 0, color );
#endif

    /* Enable this to draw the bounding box around the component to validate
     * the bounding box calculations. */
#if 0
    EDA_Rect bBox = LibEntry->GetBoundaryBox( Multi, convert );
    GRRect( &panel->m_ClipBox, dc, bBox.GetOrigin().x, bBox.GetOrigin().y,
            bBox.GetEnd().x, bBox.GetEnd().y, 0, LIGHTMAGENTA );
#endif
}


void LIB_COMPONENT::RemoveDrawItem( LibEDA_BaseStruct* item,
                                    WinEDA_DrawPanel* panel,
                                    wxDC* dc )
{
    wxASSERT( item != NULL );

    LibEDA_BaseStruct* prevItem = m_Drawings;

    if( dc != NULL )
        item->Draw( panel, dc, wxPoint( 0, 0 ), -1, g_XorMode, NULL,
                    DefaultTransformMatrix );

    if( m_Drawings == item )
    {
        m_Drawings = item->Next();
        SAFE_DELETE( item );
        return;
    }

    while( prevItem )
    {
        if( prevItem->Next() == item )
        {
            prevItem->SetNext( item->Next() );
            SAFE_DELETE( item );
            break;
        }

        prevItem = prevItem->Next();
    }
}


/**
 * Function Save
 * writes the data structures for this object out to a FILE in "*.brd" format.
 * @param aFile The FILE to write to.
 * @return bool - true if success writing else false.
 */
bool LIB_COMPONENT::Save( FILE* aFile )
{
    LibEDA_BaseStruct* DrawEntry;
    LibDrawField*      Field;

    if( Type != ROOT )  // should not happen, but just in case
        return false;

    /* First line: it s a comment (component name for readers) */
    if( fprintf( aFile, "#\n# %s\n#\n", CONV_TO_UTF8( m_Name.m_Text ) ) < 0 )
        return false;

    /* Save data */
    if( fprintf( aFile, "DEF" ) < 0 )
        return false;

    if( (m_Name.m_Attributs & TEXT_NO_VISIBLE) == 0 )
    {
        if( fprintf( aFile, " %s", CONV_TO_UTF8( m_Name.m_Text ) ) < 0 )
            return false;
    }
    else
    {
        if( fprintf( aFile, " ~%s", CONV_TO_UTF8( m_Name.m_Text ) ) < 0 )
            return false;
    }

    if( !m_Prefix.m_Text.IsEmpty() )
    {
        if( fprintf( aFile, " %s", CONV_TO_UTF8( m_Prefix.m_Text ) ) < 0 )
            return false;
    }
    else
    {
        if( fprintf( aFile, " ~" ) < 0 )
            return false;
    }

    if( fprintf( aFile, " %d %d %c %c %d %c %c\n",
                 0, m_TextInside,
                 m_DrawPinNum ? 'Y' : 'N',
                 m_DrawPinName ? 'Y' : 'N',
                 m_UnitCount, m_UnitSelectionLocked ? 'L' : 'F',
                 m_Options == ENTRY_POWER ? 'P' : 'N' ) < 0 )
        return false;

    if( !SaveDateAndTime( aFile ) || !m_Prefix.Save( aFile )
        || !m_Name.Save( aFile ) )
        return false;

    for( Field = m_Fields; Field != NULL; Field = Field->Next() )
    {
        if( Field->m_Text.IsEmpty() && Field->m_Name.IsEmpty() )
            continue;
        if( !Field->Save( aFile ) )
            return false;
    }

    /* Save the alias list: a line starting by "ALIAS" */
    if( m_AliasList.GetCount() != 0 )
    {
        if( fprintf( aFile, "ALIAS" ) < 0 )
            return false;

        for( size_t ii = 0; ii < m_AliasList.GetCount(); ii++ )
        {
            if( fprintf( aFile, " %s", CONV_TO_UTF8( m_AliasList[ii] ) ) < 0 )
                return false;
        }

        if( fprintf( aFile, "\n" ) < 0 )
            return false;
    }

    /* Write the footprint filter list */
    if( m_FootprintList.GetCount() != 0 )
    {
        if( fprintf( aFile, "$FPLIST\n" ) < 0 )
            return false;

        for( size_t ii = 0; ii < m_FootprintList.GetCount(); ii++ )
        {
            if( fprintf( aFile, " %s\n",
                         CONV_TO_UTF8( m_FootprintList[ii] ) ) < 0 )
                return false;
        }

        if( fprintf( aFile, "$ENDFPLIST\n" ) < 0 )
            return false;
    }

    /* Save graphics items (including pins) */
    if( m_Drawings )
    {
        /* we sort the draw items, in order to have an edition more easy,
         *  when a file editing "by hand" is made */
        SortDrawItems();

        if( fprintf( aFile, "DRAW\n" ) < 0 )
            return false;

        DrawEntry = m_Drawings;
        while( DrawEntry )
        {
            if( !DrawEntry->Save( aFile ) )
                return false;
            DrawEntry = DrawEntry->Next();
        }

        if( fprintf( aFile, "ENDDRAW\n" ) < 0 )
            return false;
    }

    if( fprintf( aFile, "ENDDEF\n" ) < 0 )
        return false;

    return true;
}

bool LIB_COMPONENT::Load( FILE* file, char* line, int* lineNum,
                          wxString& errorMsg )
{
    int      unused;
    char*    p;
    char*    name;
    char*    prefix = NULL;

    bool     Res;
    wxString Msg;

    p = strtok( line, " \t\r\n" );

    if( strcmp( p, "DEF" ) != 0 )
    {
        errorMsg.Printf( wxT( "DEF command expected in line %d, aborted." ),
                         *lineNum );
        return false;
    }

    /* Read DEF line: */
    char drawnum = 0;
    char drawname = 0;

    if( ( name = strtok( NULL, " \t\n" ) ) == NULL       /* Part name: */
        || ( prefix = strtok( NULL, " \t\n" ) ) == NULL  /* Prefix name: */
        || ( p = strtok( NULL, " \t\n" ) ) == NULL       /* NumOfPins: */
        || sscanf( p, "%d", &unused ) != 1
        || ( p = strtok( NULL, " \t\n" ) ) == NULL       /* TextInside: */
        || sscanf( p, "%d", &m_TextInside ) != 1
        || ( p = strtok( NULL, " \t\n" ) ) == NULL       /* DrawNums: */
        || sscanf( p, "%c", &drawnum ) != 1
        || ( p = strtok( NULL, " \t\n" ) ) == NULL       /* DrawNums: */
        || sscanf( p, "%c", &drawname ) != 1
        || ( p = strtok( NULL, " \t\n" ) ) == NULL       /* m_UnitCount: */
        || sscanf( p, "%d", &m_UnitCount ) != 1 )
    {
        errorMsg.Printf( _( "Wrong DEF format in line %d, skipped." ),
                         *lineNum );
        while( GetLine( file, line, lineNum, 1024 ) )
        {
            p = strtok( line, " \t\n" );
            if( stricmp( p, "ENDDEF" ) == 0 )
                break;
        }

        return false;
    }

    m_DrawPinNum  = (drawnum == 'N') ? FALSE : TRUE;
    m_DrawPinName = (drawname == 'N') ? FALSE : TRUE;

    /* Copy part name and prefix. */
    strupper( name );
    if( name[0] != '~' )
        m_Name.m_Text = CONV_FROM_UTF8( name );
    else
    {
        m_Name.m_Text = CONV_FROM_UTF8( &name[1] );
        m_Name.m_Attributs |= TEXT_NO_VISIBLE;
    }

    if( strcmp( prefix, "~" ) == 0 )
    {
        m_Prefix.m_Text.Empty();
        m_Prefix.m_Attributs |= TEXT_NO_VISIBLE;
    }
    else
        m_Prefix.m_Text = CONV_FROM_UTF8( prefix );

    // Copy optional infos
    if( ( p = strtok( NULL, " \t\n" ) ) != NULL && *p == 'L' )
        m_UnitSelectionLocked = TRUE;
    if( ( p = strtok( NULL, " \t\n" ) ) != NULL  && *p == 'P' )
        m_Options = ENTRY_POWER;

    /* Read next lines */
    while( GetLine( file, line, lineNum, 1024 ) )
    {
        p = strtok( line, " \t\n" );

        /* This is the error flag ( if an error occurs, Res = FALSE) */
        Res = true;

        if( (line[0] == 'T') && (line[1] == 'i') )
            Res = LoadDateAndTime( line );
        else if( line[0] == 'F' )
            Res = LoadField( line, Msg );
        else if( strcmp( p, "ENDDEF" ) == 0 )
            break;
        else if( strcmp( p, "DRAW" ) == 0 )
            Res = LoadDrawEntries( file, line, lineNum, Msg );
        else if( strncmp( p, "ALIAS", 5 ) == 0 )
        {
            p = strtok( NULL, "\r\n" );
            Res = LoadAliases( p, errorMsg );
        }
        else if( strncmp( p, "$FPLIST", 5 ) == 0 )
            Res = LoadFootprints( file, line, lineNum, Msg );

        /* End line or block analysis: test for an error */
        if( !Res )
        {
            if( Msg.IsEmpty() )
                errorMsg.Printf( wxT( "error occurred at line %d " ), *lineNum );
            else
                errorMsg.Printf( wxT( "error <%s> occurred at line %d " ),
                                 ( const wxChar* ) Msg, *lineNum );
            return false;
        }
    }

    /* If we are here, this part is O.k. - put it in: */
    SortDrawItems();

    return true;
}


bool LIB_COMPONENT::LoadDrawEntries( FILE* f, char* line,
                                     int* lineNum, wxString& errorMsg )
{
    LibEDA_BaseStruct* newEntry = NULL;
    LibEDA_BaseStruct* headEntry = NULL;
    LibEDA_BaseStruct* tailEntry = NULL;

    while( true )
    {
        if( GetLine( f, line, lineNum, 1024 ) == NULL )
        {
            errorMsg = _( "file ended prematurely loading component draw element" );
            return false;
        }

        if( strncmp( line, "ENDDRAW", 7 ) == 0 )
            break;

        newEntry = NULL;

        switch( line[0] )
        {
        case 'A':    /* Arc */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawArc(this);
            break;

        case 'C':    /* Circle */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawCircle(this);
            break;

        case 'T':    /* Text */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawText(this);
            break;

        case 'S':    /* Square */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawSquare(this);
            break;

        case 'X':    /* Pin Description */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawPin(this);
            break;

        case 'P':    /* Polyline */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawPolyline(this);
            break;

        case 'B':    /* Bezier Curves */
            newEntry = ( LibEDA_BaseStruct* ) new LibDrawBezier(this);
            break;

        default:
            errorMsg.Printf( wxT( "undefined DRAW command %c" ), line[0] );
            m_Drawings = headEntry;
            return false;
        }

        if( !newEntry->Load( line, errorMsg ) )
        {
            errorMsg.Printf( wxT( "error <%s> in DRAW command %c" ),
                             ( const wxChar* ) errorMsg, line[0] );
            SAFE_DELETE( newEntry );

            /* Flush till end of draw section */
            do
            {
                if( GetLine( f, line, lineNum, 1024 ) == NULL )
                {
                    errorMsg = _( "file ended prematurely while attempting \
to flush to end of drawing section." );
                    return false;
                }
            } while( strncmp( line, "ENDDRAW", 7 ) != 0 );

            SAFE_DELETE( headEntry );
            return false;
        }
        else
        {
            if( headEntry == NULL )
                headEntry = tailEntry = newEntry;
            else
            {
                tailEntry->SetNext( newEntry );
                tailEntry = newEntry;
            }
        }
    }

    m_Drawings = headEntry;
    return true;
}


bool LIB_COMPONENT::LoadAliases( char* line, wxString& errorMsg )
{
    char* text = strtok( line, " \t\r\n" );

    while( text )
    {
        m_AliasList.Add( CONV_FROM_UTF8( text ) );
        text = strtok( NULL, " \t\r\n" );
    }

    return true;
}


bool LIB_COMPONENT::LoadField( char* line, wxString& errorMsg )
{
    LibDrawField* field = new LibDrawField( this );

    if ( !field->Load( line, errorMsg ) )
    {
        SAFE_DELETE( field );
        return false;
    }

    if( field->m_FieldId == REFERENCE )
    {
        m_Prefix = *field;
        SAFE_DELETE( field );
    }
    else if ( field->m_FieldId == VALUE )
    {
        m_Name = *field;
        SAFE_DELETE( field );
    }
    else
    {
        m_Fields.PushBack( field );
    }

    return true;
}


bool LIB_COMPONENT::LoadFootprints( FILE* file, char* line,
                                    int* lineNum, wxString& errorMsg )
{
    while( true )
    {
        if( GetLine( file, line, lineNum, 1024 ) == NULL )
        {
            errorMsg = wxT( "file ended prematurely while loading footprints" );
            return false;
        }

        if( stricmp( line, "$ENDFPLIST" ) == 0 )
            break;

        m_FootprintList.Add( CONV_FROM_UTF8( line + 1 ) );
    }

    return true;
}


/* TODO translate comment to english TODO
 * Trie les �l�ments graphiques d'un composant lib pour am�liorer
 *  le trac�:
 *  items remplis en premier, pins en dernier
 *  En cas de superposition d'items, c'est plus lisible
 */
void LIB_COMPONENT::SortDrawItems()
{
    LibEDA_BaseStruct** Bufentry, ** BufentryBase, * Entry = m_Drawings;
    int ii, nbitems;

    if( Entry == NULL )
        return; /* Pas d'alias pour ce composant */
    /* calcul du nombre d'items */
    for( nbitems = 0; Entry != NULL; Entry = Entry->Next() )
        nbitems++;

    BufentryBase =
        (LibEDA_BaseStruct**) MyZMalloc( (nbitems + 1) *
                                         sizeof(LibEDA_BaseStruct*) );

    /* memorisation du chainage : */
    for( Entry = m_Drawings, ii = 0; Entry != NULL; Entry = Entry->Next() )
        BufentryBase[ii++] = Entry;

    /* Tri du chainage */
    qsort( BufentryBase, nbitems, sizeof(LibEDA_BaseStruct*), SortItemsFct );

    /* Mise a jour du chainage. Remarque:
     *  le dernier element de BufEntryBase (BufEntryBase[nbitems]) est NULL*/
    m_Drawings = *BufentryBase;
    Bufentry   = BufentryBase;
    for( ii = 0; ii < nbitems; ii++ )
    {
        (*Bufentry)->SetNext( *(Bufentry + 1) );
        Bufentry++;
    }

    MyFree( BufentryBase );
}


/**********************************************************************/
/* Return the component boundary box ( in user coordinates )
 *  The unit Unit, and the shape Convert are considered.
 *  If Unit == 0, Unit is not used
 *  if Convert == 0 Convert is non used
 **/
/**********************************************************************/
EDA_Rect LIB_COMPONENT::GetBoundaryBox( int Unit, int Convert )
{
    LibEDA_BaseStruct* DrawEntry;
    EDA_Rect           bBox( wxPoint( 0, 0 ), wxSize( 0, 0 ) );

    for( DrawEntry = m_Drawings; DrawEntry != NULL;
         DrawEntry = DrawEntry->Next() )
    {
        if( DrawEntry->m_Unit > 0 )  // The item is non common to units
            if( ( m_UnitCount > 1 ) && ( Unit > 0 )
                && ( Unit != DrawEntry->m_Unit ) )
                continue;
        if( DrawEntry->m_Convert > 0 )  // The item is not common to all convert
            if( ( Convert > 0 ) && ( Convert != DrawEntry->m_Convert ) )
                continue;

        bBox.Merge( DrawEntry->GetBoundingBox() );
    }

    return bBox;
}


/** Function SetFields
 * initialize fields from a vector of fields
 * @param aFields a std::vector <LibDrawField> to import.
 */
void LIB_COMPONENT::SetFields( const std::vector <LibDrawField> aFields )
{
    // Init basic fields (Value = name in lib, and reference):
    aFields[VALUE].Copy( &m_Name );
    aFields[REFERENCE].Copy( &m_Prefix );

    // Remove others fields:
    m_Fields.DeleteAll();

    for( unsigned ii = FOOTPRINT; ii < aFields.size(); ii++ )
    {
        bool create = FALSE;
        if( !aFields[ii].m_Text.IsEmpty() )
            create = TRUE;
        if( !aFields[ii].m_Name.IsEmpty()
            && ( aFields[ii].m_Name != ReturnDefaultFieldName( ii ) ) )
            create = TRUE;
        if( create )
        {
            LibDrawField*Field = new LibDrawField( this, ii );
            aFields[ii].Copy( Field );
            m_Fields.PushBack( Field );
        }
    }

    /* for a user field (FieldId >= FIELD1), if a field value is void,
     * fill it with "~" because for a library component a void field is not
     * a very good idea  (we do not see anything...) and in schematic this
     * text is like a void text and for non editable names, remove the name
     * (set to the default name)
     */
    for( LibDrawField* Field = m_Fields; Field; Field = Field->Next() )
    {
        Field->SetParent( this );
        if( Field->m_FieldId >= FIELD1 )
        {
            if( Field->m_Text.IsEmpty() )
                Field->m_Text = wxT( "~" );
        }
        else
            Field->m_Name.Empty();
    }
}


/*
 * lit date et time de modif composant sous le format:
 *  "Ti yy/mm/jj hh:mm:ss"
 */
bool LIB_COMPONENT::SaveDateAndTime( FILE* file )
{
    int year, mon, day, hour, min, sec;

    if( m_LastDate == 0 )
        return true;

    sec  = m_LastDate & 63;
    min  = ( m_LastDate >> 6 ) & 63;
    hour = ( m_LastDate >> 12 ) & 31;
    day  = ( m_LastDate >> 17 ) & 31;
    mon  = ( m_LastDate >> 22 ) & 15;
    year = ( m_LastDate >> 26 ) + 1990;

    if ( fprintf( file, "Ti %d/%d/%d %d:%d:%d\n",
                  year, mon, day, hour, min, sec ) < 0 )
        return false;

    return true;
}

/* lit date et time de modif composant sous le format:
 *  "Ti yy/mm/jj hh:mm:ss"
 */
bool LIB_COMPONENT::LoadDateAndTime( char* Line )
{
    int   year, mon, day, hour, min, sec;
    char* text;

    year = mon = day = hour = min = sec = 0;
    text = strtok( Line, " \r\t\n" );
    text = strtok( NULL, " \r\t\n" );  // text pointe donnees utiles

    if (sscanf( Line, "%d/%d/%d %d:%d:%d",
                &year, &mon, &day, &hour, &min, &sec ) != 6 )
        return false;

    m_LastDate = ( sec & 63 ) + ( ( min & 63 ) << 6 ) +
        ( ( hour & 31 ) << 12 ) + ( ( day & 31 ) << 17 ) +
        ( ( mon & 15 ) << 22 ) + ( ( year - 1990 ) << 26 );

    return true;
}


void LIB_COMPONENT::SetOffset( const wxPoint& offset )
{
    LibEDA_BaseStruct* DrawEntry;

    m_Name.SetOffset( offset );
    m_Prefix.SetOffset( offset );

    for( LibDrawField* field = m_Fields; field != NULL; field = field->Next() )
    {
        field->SetOffset( offset );
    }

    DrawEntry = m_Drawings;

    while( DrawEntry )
    {
        DrawEntry->SetOffset( offset );
        DrawEntry = DrawEntry->Next();
    }
}


void LIB_COMPONENT::RemoveDuplicateDrawItems()
{
    LibEDA_BaseStruct* DEntryRef;
    LibEDA_BaseStruct* DEntryCompare;
    bool  deleted;

    DEntryRef = m_Drawings;

    while( DEntryRef )
    {
        if( DEntryRef->Next() == NULL )
            return;
        DEntryCompare = DEntryRef->Next();

        if( DEntryCompare == NULL )
            return;

        deleted = false;

        while( DEntryCompare )
        {
            if( DEntryRef == DEntryCompare )
            {
                RemoveDrawItem( DEntryRef, NULL, NULL );
                deleted = true;
                break;
            }

            DEntryCompare = DEntryCompare->Next();
        }

        if( !deleted )
            DEntryRef = DEntryRef->Next();
        else
            DEntryRef = m_Drawings;
    }
}


bool LIB_COMPONENT::HasConversion() const
{
    LibEDA_BaseStruct* entry;

    for( entry = m_Drawings; entry != NULL; entry = entry->Next() )
    {
        if( entry->m_Convert > 1 )
            return true;
    }

    return false;
}


/**
 * Function SaveDoc
 * writes the doc info out to a FILE in "*.dcm" format.
 * Only non empty fields are written.
 * If all fields are empty, does not write anything
 * @param aFile The FILE to write to.
 * @return bool - true if success writing else false.
 */
bool CMP_LIB_ENTRY::SaveDoc( FILE* aFile )
{
    if( m_Doc.IsEmpty() && m_KeyWord.IsEmpty() && m_DocFile.IsEmpty() )
        return true;

    /* Generation des lignes utiles */
    if( fprintf( aFile, "#\n$CMP %s\n", CONV_TO_UTF8( m_Name.m_Text ) ) < 0 )
        return false;

    if( ! m_Doc.IsEmpty()
        && fprintf( aFile, "D %s\n", CONV_TO_UTF8( m_Doc ) ) < 0 )
        return false;

    if( ! m_KeyWord.IsEmpty()
        && fprintf( aFile, "K %s\n", CONV_TO_UTF8( m_KeyWord ) ) < 0 )
        return false;

    if( ! m_DocFile.IsEmpty()
        && fprintf( aFile, "F %s\n", CONV_TO_UTF8( m_DocFile ) ) < 0 )
        return false;

    if( fprintf( aFile, "$ENDCMP\n" ) < 0 )
        return false;

    return true;
}
