#include "dnf.h"
#include <iostream>

DNF::DNF( std::string strData ) :
    m_numPatch( 0 )
{    
    setData( strData );
}

DNF::DNF() : m_numPatch( 0 ), m_base( 0 )
{

}

void DNF::setData( std::string strData )
{
    m_base = static_cast <int> ( std::ceil( log2( strData.size() ) ) );

    for ( size_t i = 0; i < strData.length(); i++ )
    {
        char sym = strData.at( i );

        if ( sym == '1' || sym == '-' )
        {
            m_data.push_back( std::make_shared <Impl> ( i ) );

            if( sym == '-' )
                m_data.back()->setUndefined( true );
        }
    }
}

void DNF::makeDDNF()
{
    if( m_data.empty() )
        return;

    std::vector < std::shared_ptr <Impl> > dataNew;
    std::vector < std::shared_ptr <Impl> > dataOld;

    std::copy( m_data.begin(), m_data.end(), std::back_inserter( dataOld  ) );

    while(1)
    {
        m_numPatch++;

        for ( size_t i = 0; i < dataOld.size() - 1; i++ )
        {
            for ( size_t j = i + 1; j < dataOld.size(); j++ )
            {
                std::shared_ptr <Impl> newImpl = Impl::patch( dataOld.at( i ), dataOld.at( j ) );

                if ( newImpl != nullptr )
                    dataNew.push_back( newImpl );
            }
        }

        std::copy_if( dataOld.begin(), dataOld.end(),
                     std::back_inserter( m_ddnf ),
                     [&] ( std::shared_ptr <Impl> impl ){ return !( impl->getPw() ); } );

        if ( dataNew.empty() )
        {
            m_ddnf.sort();
            m_ddnf.unique();

            break;
        }
        else
            dataOld = std::move( dataNew );
    }
}

/*
 * finds first absorbed implicant in the column and
 * marks corresponding impl as 'covering' (set 'covers' member to true)
*/
void pushOneToMDNF( std::vector < bool > column,
                    std::vector < std::shared_ptr <Impl> > ddnf,
                    std::list   < std::shared_ptr < Impl > > & mdnf)
{
    std::vector < bool >::iterator absorbedIt = std::find( column.begin(),
                                                           column.end(),
                                                           true);

    int index = absorbedIt - column.begin(); //get index of basic impl

    if( std::find( mdnf.begin(), mdnf.end(), ddnf.at( index ) ) == mdnf.end() ) //if there is no more such implicant yet
    {
        mdnf.push_back( ddnf.at( index ) );
        ddnf.at( index )->setCovers( true );
    }
}

template<typename iter_t>
void DNF::fit( std::vector < std::shared_ptr <Impl> > ddnf,
               std::list   < std::shared_ptr < Impl > > & mdnf,
               iter_t matrIt,
               iter_t endIt )
{
    for( ; matrIt != endIt; matrIt++ )
    {
        std::vector < bool > column = *matrIt;
        int absorbedNum = std::accumulate( column.begin(), column.end(), 0 );

        if( absorbedNum == 1 ) // there is only one absorbed impl inthis column
            pushOneToMDNF( column, ddnf, mdnf );
        else
        {
            std::list < std::shared_ptr < Impl > > coveredImpl;
            std::list < std::shared_ptr < Impl > >::iterator basicIt;
            std::list < std::shared_ptr < Impl > >::iterator coveringIt;

            for( size_t i = 0; i < column.size(); i++ )
            {
                if( column.at( i ) ) //if implicant at i covered by current constituent
                    coveredImpl.push_back( ddnf.at( i ) );
            }

            basicIt = std::find_if( coveredImpl.begin(), coveredImpl.end(), []( std::shared_ptr < Impl > impl )
                                                                              { return impl->isBasic(); } ); //trying to find basic impl

            if( basicIt != coveredImpl.end() ) //if there is basic impl in this column
            {
                if( std::find( mdnf.begin(), mdnf.end(), *basicIt ) == mdnf.end() ) //if there is no more such implicant yet
                {
                    mdnf.push_back( *basicIt );
                    ( *basicIt )->setCovers( true );
                }
            }
            else
            {
                coveringIt = std::find_if( coveredImpl.begin(), coveredImpl.end(), []( std::shared_ptr < Impl > impl ) //find impl that included in the mdnf
                                                                                     { return impl->covers(); } );

                if( coveringIt == coveredImpl.end() ) // if there is no impl included in the mdnf
                    pushOneToMDNF( column, ddnf, mdnf );
            }
        }
    }
}

void DNF::absorbDDNF()
{
    if( m_data.empty() )
        return;

    std::vector < std::shared_ptr <Impl> > definedConstituents;
    std::vector < std::shared_ptr <Impl> > ddnfVec;

    std::copy(  m_ddnf.begin(), m_ddnf.end(), std::back_inserter( ddnfVec ) );

    std::sort( ddnfVec.begin(), ddnfVec.end(), []( std::shared_ptr < Impl > a, std::shared_ptr < Impl > b ) //sort to get the shortest implicants at
                                                 { return ( Impl::count( a->getP() ) > Impl::count( b->getP() ) ) ; } ); // the beginning

    std::copy_if( m_data.begin(), m_data.end(),
                 std::back_inserter( definedConstituents ),
                 [&] ( std::shared_ptr <Impl> impl ){ return !( impl->isUndefined() ); } );

    std::vector < std::vector < bool > > absorbedMatrix( definedConstituents.size(),
                                                         std::vector < bool > ( m_ddnf.size(), false ) ); //implication matrix

    for( size_t i = 0; i < definedConstituents.size(); i++ ) //matrix initializing loop
    {
        std::shared_ptr <Impl> cons = definedConstituents.at( i );
        uint32_t absorbedCnt = 0;

        for ( size_t j = 0; j < ddnfVec.size(); j++ )
        {
            std::shared_ptr <Impl> impl = ddnfVec.at( j );

            if( ( cons->getNum() & ~( impl->getP() ) ) == impl->getNum() )
            {
                absorbedMatrix.at( i ).at( j ) = true;
                absorbedCnt++;
            }
        }

        if( absorbedCnt == 1 )
        {
            std::vector < bool >::iterator absorbedIt = std::find( absorbedMatrix.at( i ).begin(),
                                                           absorbedMatrix.at( i ).end(),
                                                           true);

            int index = absorbedIt - absorbedMatrix.at( i ).begin();
            ddnfVec.at( index )->setBasic( true );
        }
        else if ( !absorbedCnt )
            return;
    }

    std::list < std::shared_ptr < Impl > > mdnfForward;
    std::list < std::shared_ptr < Impl > > mdnfReverse;

    fit( ddnfVec, mdnfReverse, absorbedMatrix.rbegin(), absorbedMatrix.rend() );

    for( std::shared_ptr <Impl> & impl : ddnfVec )
        impl->setCovers( false ); //reset covers flags to check it again at the next time

    fit( ddnfVec, mdnfForward, absorbedMatrix.begin(), absorbedMatrix.end() );

    m_mdnf = ( ( mdnfForward.size() < mdnfReverse.size() ) ) ? std::move( mdnfForward ) : std::move( mdnfReverse );
}

void DNF::minimize()
{
    makeDDNF();
    absorbDDNF();
}

std::string decToBinStr( int val, int len )
{
    std::string binary = "";
    int mask = 1;

    for( int i = 0; i < len; i++ )
    {
        if( ( mask & val ) >= 1 )
            binary = "1" + binary;
        else
            binary = "0" + binary;
        mask <<= 1;
    }

    return binary;
}

void DNF::print( std::ostream & stream )
{
    if( m_mdnf.empty() )
        return;

    for( std::shared_ptr <Impl> impl : m_mdnf )
    {
        std::string P_str = decToBinStr( impl->getP(), m_base );
        std::string Num_str = decToBinStr( impl->getNum(), m_base );

        std::string impl_str;
        impl_str.resize( m_base );

        std::transform( P_str.begin(), P_str.end(), impl_str.begin(),
                        []( char sym ){ return ( sym == '1' ) ? '-' : '\0'; } );

        std::string::reverse_iterator num_iter = Num_str.rbegin();

        for( std::string::reverse_iterator impl_iter = impl_str.rbegin(); impl_iter != impl_str.rend(); impl_iter++, num_iter++ )
        {
            if( *impl_iter != '-' )
                *impl_iter = *num_iter;
        }

        stream << impl_str << std::endl;
    }
}

std::list < std::string > DNF::print( )
{
    std::list < std::string > mdnfList;

    std::list < std::shared_ptr < Impl > > dnf;

    if( m_mdnf.empty() )
        std::copy( m_data.begin(), m_data.end(), std::back_inserter( dnf ) );
    else
        std::copy( m_mdnf.begin(), m_mdnf.end(), std::back_inserter( dnf ) );

    for( std::shared_ptr <Impl> impl : dnf )
    {
        std::string P_str = decToBinStr( impl->getP(), m_base );
        std::string Num_str = decToBinStr( impl->getNum(), m_base );

        std::string impl_str;
        impl_str.resize( m_base );

        std::transform( P_str.begin(), P_str.end(), impl_str.begin(),
                        []( char sym ){ return ( sym == '1' ) ? '-' : '\0'; } );

        std::string::reverse_iterator num_iter = Num_str.rbegin();

        for( std::string::reverse_iterator impl_iter = impl_str.rbegin(); impl_iter != impl_str.rend(); impl_iter++, num_iter++ )
        {
            if( *impl_iter != '-' )
                *impl_iter = *num_iter;
        }

        mdnfList.push_back( impl_str );
    }
    return mdnfList;
}

std::list < std::shared_ptr <Impl> > DNF::getMDNF()
{
    return m_mdnf;
}

DNF::~DNF()
{

}
