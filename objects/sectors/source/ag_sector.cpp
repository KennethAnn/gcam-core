/*! 
* \file ag_sector.cpp
* \ingroup Objects
* \brief AgSector class source file.
* \author Josh Lurz
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"
#include <iostream>
#include <fstream>
#include "sectors/include/ag_sector.h"
#include "util/base/include/xml_helper.h"
#include "marketplace/include/marketplace.h"
#include "marketplace/include/imarket_type.h"
#include "util/base/include/model_time.h"
#include "containers/include/scenario.h"
#include "util/base/include/configuration.h"

// Fortran calls.
#if(__HAVE_FORTRAN__)
extern "C" { void _stdcall SETGNP( int&, double[] ); };
extern "C" { double _stdcall GETGNP( int&, int& ); };
extern "C" { void _stdcall SETPOP( int&, double[] ); };
extern "C" { double _stdcall GETPOP( int&, int& ); };
extern "C" { void _stdcall SETBIOMASSPRICE( double[] ); };
extern "C" { double _stdcall GETBIOMASSPRICE( ); };
extern "C" { void _stdcall AG2RUN( double[], int&, int&, double[], double[] ); };
extern "C" { double _stdcall AG2CO2EMISSIONS( int&, int& ); };
extern "C" { void _stdcall AG2LINKOUT( void ); };
#endif

using namespace std;
using namespace xercesc;

extern Scenario* scenario;
// static initialize.
const string AgSector::XML_NAME = "agsector";

int AgSector::regionCount = 0;
const int AgSector::numAgMarkets = 12;
bool AgSector::init = false;
map<string, int> AgSector::nameToIndiceMap;
vector<string> AgSector::marketNameVector;
map<int, string> AgSector::indiceToNameMap;

//! Constructor
AgSector::AgSector() {
   
   regionNumber = regionCount;
   regionCount++;
   biomassPrice = 0;
   
   if( !init ){
      staticInitialize();
   }
}

//! Destructor
AgSector::~AgSector(){
    regionCount--;
}

//! Initialize static data members.
void AgSector::staticInitialize(){
   init = true;
   // Initialize marketNameVector(Really should be static)
   marketNameVector.push_back( "wood" );
   marketNameVector.push_back( "forward wood" );
   marketNameVector.push_back( "food grains" );
   marketNameVector.push_back( "coarse grains" );
   marketNameVector.push_back( "oil crops" );
   marketNameVector.push_back( "misc crops" );
   marketNameVector.push_back( "pasture" );
   
   // Initialize nameToIndiceMap
   nameToIndiceMap[ "crude oil" ] = 0;
   nameToIndiceMap[ "natural gas" ] = 1;
   nameToIndiceMap[ "coal" ] = 2;
   nameToIndiceMap[ "biomass" ] = 3;
   nameToIndiceMap[ "carbon" ] = 4;
   nameToIndiceMap[ "wood" ] = 5;
   nameToIndiceMap[ "forward wood" ] = 6;
   nameToIndiceMap[ "food grains" ] = 7;
   nameToIndiceMap[ "coarse grains" ] = 8;
   nameToIndiceMap[ "oil crops" ] = 9;
   nameToIndiceMap[ "misc crops" ] = 10;
   nameToIndiceMap[ "pasture" ] = 11;
   
   indiceToNameMap[ 0 ] = "crude oil";
   indiceToNameMap[ 1 ] = "natural gas";
   indiceToNameMap[ 2 ] = "coal";
   indiceToNameMap[ 3 ] = "biomass";
   indiceToNameMap[ 4 ] = "carbon";
   indiceToNameMap[ 5 ] = "wood";
   indiceToNameMap[ 6 ] = "forward wood";
   indiceToNameMap[ 7 ] = "food grains";
   indiceToNameMap[ 8 ] = "coarse grains";
   indiceToNameMap[ 9 ] = "oil crops";
   indiceToNameMap[ 10 ] = "misc crops";
   indiceToNameMap[ 11 ] = "pasture";
   
}

//! Return the number of markets the AgLU model uses.
int AgSector::getNumAgMarkets() {
   return numAgMarkets;
}

//! Clear all data members.
void AgSector::clear() {
   regionNumber = 0;
   name = "";
   gdp.clear();
   population.clear();
   CO2Emissions.clear();
   prices.clear();
   supplies.clear();
   demands.clear();
}

//! Initialize the object with XML data.
void AgSector::XMLParse( const DOMNode* node ) {
   const Modeltime* modeltime = scenario->getModeltime();
   
   CO2Emissions.resize( modeltime->getmaxper() );
   prices.resize( modeltime->getmaxper() );
   supplies.resize( modeltime->getmaxper() );
   demands.resize( modeltime->getmaxper() );
   
   for( int i = 0; i < modeltime->getmaxper(); i++ ) {
      prices[ i ].resize( numAgMarkets );
      supplies[ i ].resize( numAgMarkets );
      demands[ i ].resize( numAgMarkets );
   }
}

//! Output the results in XML format.
void AgSector::toInputXML( ostream& out, Tabs* tabs ) const {
   XMLWriteOpeningTag( getXMLName(), out, tabs, name );
   
   // write the xml for the class members.
   XMLWriteElement( regionNumber, "regionNumber", out, tabs );
   XMLWriteElement( numAgMarkets, "numAgMarkets", out, tabs );
   
   const Modeltime* modeltime = scenario->getModeltime();
   for( unsigned int iter = 0; iter < gdp.size(); ++iter ){
      XMLWriteElement( gdp[ iter ], "gdp", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   
   for( unsigned int iter = 0; iter < population.size(); iter++ ) {
       XMLWriteElement( population[ iter ], "population", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   
   XMLWriteElement( biomassPrice, "biomassprice", out, tabs );
   XMLWriteClosingTag( getXMLName(), out, tabs );
}

//! Print the internal variables to XML output.
void AgSector::toDebugXML( const int period, ostream& out, Tabs* tabs ) const {
   const Modeltime* modeltime = scenario->getModeltime();
   
   XMLWriteOpeningTag( getXMLName(), out, tabs, name );
   
   // write the xml for the class members.
   // write out the market string.
   XMLWriteElement( regionNumber, "regionNumber", out, tabs );
   XMLWriteElement( numAgMarkets, "numAgMarkets", out, tabs );
   
   for( int iter = 0; iter < static_cast<int>( gdp.size() ); iter++ ){
      XMLWriteElement( gdp[ iter ], "gdp", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   
   #if(__HAVE_FORTRAN__)
   int tempRegion = regionNumber; // Needed b/c function is constant.
   for ( int iter = 0; iter < modeltime->getmaxper(); iter++ ){
      XMLWriteElement( GETGNP( tempRegion, iter ), "gdpFromFortran", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   for ( int iter = 1; iter < modeltime->getmaxper(); iter++ ){
      XMLWriteElement( GETPOP( tempRegion, iter ), "popFromFortran", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   XMLWriteElement( GETBIOMASSPRICE(), "biomasspriceFromFortran", out, tabs );
   #endif
   
   for( int iter = 0; iter < static_cast<int>( population.size() ); iter++ ) {
      XMLWriteElement( population[ iter ], "population", out, tabs, modeltime->getper_to_yr( iter ) );
   }
   
   XMLWriteElement( biomassPrice, "biomassprice", out, tabs );
   
   for( int iter = 0; iter < static_cast<int>( prices[ period ].size() ); iter++ ) {
      XMLWriteElement( prices[ period ][ iter ], "prices", out, tabs, modeltime->getper_to_yr( period ) );
   }
   
   for( int iter = 0; iter < static_cast<int>( supplies[ period ].size() ); iter++ ) {
      XMLWriteElement( supplies[ period ][ iter ], "supplies", out, tabs, modeltime->getper_to_yr( period ) );
   }	
   for( int iter = 0; iter < static_cast<int>( demands[ period ].size() ); iter++ ) {
      XMLWriteElement( demands[ period ][ iter ], "demands", out, tabs, modeltime->getper_to_yr( period ) );
   }
   // finished writing xml for the class members.
   XMLWriteClosingTag( getXMLName(), out, tabs );
}

/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overriden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& AgSector::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& AgSector::getXMLNameStatic() {
	return XML_NAME;
}

/*! \brief Complete the initialization
*
* This routine is only called once per model run
*
* \author Josh Lurz
* \warning markets are not necesarilly set when completeInit is called
*/
void AgSector::completeInit( const string& regionName ) {

    // Set markets for this sector
    setMarket( regionName );
}

//! Set the AgLU gdps from the regional gdp data.
void AgSector::setGNP( const vector<double>& gdpsToFortran ) {
   #if(__HAVE_FORTRAN__)
   gdp = gdpsToFortran;
   
   double* toFortran = new double[ gdpsToFortran.size() ];
   
   for ( int i = 0; i < static_cast<int>( gdpsToFortran.size() ); i++ ) {
      toFortran[ i ] = gdpsToFortran[ i ];
      // toFortran[ i ] = readGDPS[ regionNumber ][ i ];
   }
   
   SETGNP( regionNumber, toFortran );
   delete[] toFortran;
   #endif
}

//! Set the AgLU population data from the regional population data.
void AgSector::setPop( const vector<double>& popsToFortran ) {
    #if(__HAVE_FORTRAN__)
   population = popsToFortran;
   
   double* toFortran = new double[ popsToFortran.size() ];
   
   for ( int i = 0; i < static_cast<int>( popsToFortran.size() ); i++ ) {
      toFortran[ i ] = popsToFortran[ i ];
   }
   
   SETPOP( regionNumber, toFortran );
   delete[] toFortran;
   #endif
}

//! Set the AgLU biomass price from the market biomass price.
void AgSector::setBiomassPrice( const double bioPriceIn ) {
   #if(__HAVE_FORTRAN__)
   biomassPrice = bioPriceIn;
   
   double* biomassPriceArray = new double[ 1 ];
   biomassPriceArray[ 0 ] = bioPriceIn;
   
   // SETBIOMASSPRICE( biomassPriceArray ); // new
   delete[] biomassPriceArray;
   #endif
}

//! Run the underlying AgLU model.
void AgSector::runModel( const int period, const string& regionName ) {
   Marketplace* marketplace = scenario->getMarketplace();
   
   double* priceArray = new double[ numAgMarkets ];
   double* demandArray = new double[ numAgMarkets ];
   double* supplyArray = new double[ numAgMarkets ];

   for( int l = 0; l < numAgMarkets; l++ ){
       double price = marketplace->getPrice( indiceToNameMap[ l ], regionName, period, false );
       if( price == Marketplace::NO_MARKET_PRICE ){
            prices[ period ][ l ] = 0;
       }
       else {
           prices[ period ][ l ] = price;
       }
   }
   
   for( int i = 0; i < numAgMarkets; i++ ) {
      priceArray[ i ] = prices[ period ][ i ];
   }
   #if(__HAVE_FORTRAN__)
   int tempRegionNumber = regionNumber;
   int tempPeriod = period;
   AG2RUN( priceArray, tempRegionNumber, tempPeriod, demandArray, supplyArray );
   #endif
   
   for( int j = 0; j < numAgMarkets; j++ ) {
      demands[ period ][ j ] = demandArray[ j ];
      supplies[ period ][ j ] = supplyArray[ j ];
   }
   
   // set the market supplies and demands.
   for ( vector<string>::iterator k = marketNameVector.begin(); k != marketNameVector.end(); k++ ) {
        marketplace->addToDemand( *k, regionName, demands[ period ][ nameToIndiceMap[ *k ] ], period, false );
        marketplace->addToSupply( *k, regionName, supplies[ period ][ nameToIndiceMap[ *k ] ], period, false );
   }
   
   // set biomass supply
   marketplace->addToSupply( "biomass", regionName, supplies[ period ][ nameToIndiceMap[ "biomass" ] ], period );
   
   delete[] priceArray;
   delete[] demandArray;
   delete[] supplyArray;
}

//! Use the underlying model to calculate the amount of CO2 emitted.
void AgSector::carbLand( const int period, const string& regionName ) {
   #if(__HAVE_FORTRAN__)
   int tempRegionNumber = regionNumber;
   int tempPeriod = period;
   
   CO2Emissions[ period ] = AG2CO2EMISSIONS( tempPeriod, tempRegionNumber );
   #endif
}

//! Create a market for the sector.
// sjs -- added option agBioMarketSet in config file so that biomass market is not altered (so regional markets can be set elsewhere)
void AgSector::setMarket( const string& regionName ) {
    bool setAgBioMarket = Configuration::getInstance()->getBool( "agBioMarketSet" );
	 
   Marketplace* marketplace = scenario->getMarketplace();
   const Modeltime* modeltime = scenario->getModeltime();
   // Add all global markets.
   for( vector<string>::iterator i = marketNameVector.begin(); i != marketNameVector.end() - 1; i++ ) {
		// check if should set ag bio market
		if ( ( *i != "biomass") || setAgBioMarket ) {
			marketplace->createMarket( regionName, "global", *i, IMarketType::NORMAL );
            for( int per = 1; per < modeltime->getmaxper(); ++per ){
                marketplace->setMarketToSolve ( *i, regionName, per );
            }
		}
   }
   
   // Add the regional markets.
   marketplace->createMarket( regionName, regionName, marketNameVector[ 6 ], IMarketType::NORMAL );
   for( int per = 1; per < modeltime->getmaxper(); ++per ){
      marketplace->setMarketToSolve ( marketNameVector[ 6 ], regionName, per );           
   }
   // Initialize prices at a later point.
}

//! Call the Ag modules internal output subroutine
void AgSector::internalOutput() {
#if(__HAVE_FORTRAN__)
   AG2LINKOUT();
#endif
}

//! Initialize the market prices for agricultural goods. 
void AgSector::initMarketPrices( const string& regionName, const vector<double>& pricesIn ) {
   
   Marketplace* marketplace = scenario->getMarketplace();
   
   // Initialize prices.
   for( vector<string>::iterator i = marketNameVector.begin(); i != marketNameVector.end(); i++ ) {
        marketplace->setPrice( *i, regionName, pricesIn[ nameToIndiceMap[ *i ] ], 0, false );
   }
}
