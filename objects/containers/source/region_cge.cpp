/*
* LEGAL NOTICE
* This computer software was prepared by Battelle Memorial Institute,
* hereinafter the Contractor, under Contract No. DE-AC05-76RL0 1830
* with the Department of Energy (DOE). NEITHER THE GOVERNMENT NOR THE
* CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
* LIABILITY FOR THE USE OF THIS SOFTWARE. This notice including this
* sentence must appear on any copies of this computer software.
* 
* EXPORT CONTROL
* User agrees that the Software will not be shipped, transferred or
* exported into any country or used in any manner prohibited by the
* United States Export Administration Act or any other applicable
* export laws, restrictions or regulations (collectively the "Export Laws").
* Export of the Software may require some form of license or other
* authority from the U.S. Government, and failure to obtain such
* export control license may result in criminal liability under
* U.S. laws. In addition, if the Software is identified as export controlled
* items under the Export Laws, User represents and warrants that User
* is not a citizen, or otherwise located within, an embargoed nation
* (including without limitation Iran, Syria, Sudan, Cuba, and North Korea)
*     and that User is not otherwise prohibited
* under the Export Laws from receiving the Software.
* 
* Copyright 2011 Battelle Memorial Institute.  All Rights Reserved.
* Distributed as open-source under the terms of the Educational Community 
* License version 2.0 (ECL 2.0). http://www.opensource.org/licenses/ecl2.php
* 
* For further details, see: http://www.globalchange.umd.edu/models/gcam/
*
*/


/*! 
* \file region_cge.cpp
* \ingroup Objects-SGM
* \brief The RegionCGE class source file.
* \author Sonny Kim
*/

#include "util/base/include/definitions.h"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <xercesc/dom/DOMNode.hpp>

#include "containers/include/region_cge.h"
#include "demographics/include/demographic.h"
#include "sectors/include/production_sector.h"
#include "resources/include/resource.h"
#include "util/base/include/xml_helper.h"
#include "containers/include/scenario.h"
#include "util/base/include/model_time.h"
#include "marketplace/include/marketplace.h"
#include "util/base/include/configuration.h"
#include "util/base/include/util.h"
#include "util/logger/include/logger.h"
#include "sectors/include/final_demand_sector.h"
#include "sectors/include/factor_supply.h"
#include "containers/include/national_account.h"
#include "consumers/include/calc_capital_good_price_visitor.h"
#include "containers/include/iinfo.h"
// classes for reporting
#include "util/base/include/ivisitor.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;

// static initialize.
const string RegionCGE::XML_NAME = "regionCGE";

typedef std::vector<FinalDemandSector*>::iterator FinalDemandSectorIterator;
typedef std::vector<FinalDemandSector*>::const_iterator CFinalDemandSectorIterator;
typedef std::vector<FactorSupply*>::iterator FactorSupplyIterator;
typedef std::vector<FactorSupply*>::const_iterator CFactorSupplyIterator;
typedef std::vector<Sector*>::iterator SectorIterator;
typedef std::vector<Sector*>::const_iterator CSectorIterator;
typedef std::vector<AResource*>::iterator ResourceIterator;

//! Default constructor
RegionCGE::RegionCGE() {
    // Resize all vectors to maximum period
    const int maxper = scenario->getModeltime()->getmaxper();
    mNationalAccounts.resize( maxper );
}

//! Default destructor destroys sector, demsector, Resource, and population objects.
RegionCGE::~RegionCGE() {
    clear();
}

//! Clear member variables and initialize elemental members.
void RegionCGE::clear(){
    for ( FinalDemandSectorIterator demIter = finalDemandSector.begin(); demIter != finalDemandSector.end(); ++demIter ) {
        delete *demIter;
    }

    for ( FactorSupplyIterator facIter = factorSupply.begin(); facIter != factorSupply.end(); ++facIter ) {
        delete *facIter;
    }

    for( vector<NationalAccount*>::iterator iter = mNationalAccounts.begin(); iter != mNationalAccounts.end(); ++iter ) {
        delete *iter;
    }
    
    delete mCalcCapitalGoodPriceVisitor;
}

/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overridden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& RegionCGE::getXMLName() const {
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
const std::string& RegionCGE::getXMLNameStatic() {
    return XML_NAME;
}

//! Parse xml file for data
bool RegionCGE::XMLDerivedClassParse( const string& nodeName, const DOMNode* curr ) {
    if( nodeName == FinalDemandSector::getXMLNameStatic() ){
        parseContainerNode( curr, finalDemandSector, new FinalDemandSector( Region::getName() ) );
    }
    else if( nodeName == FactorSupply::getXMLNameStatic() ){
        parseContainerNode( curr, factorSupply, new FactorSupply() );
    }
    else if( nodeName == ProductionSector::getXMLNameStatic() ){
        parseContainerNode( curr, mSupplySector, new ProductionSector( mName ) );
    }
    else if( nodeName == NationalAccount::getXMLNameStatic() ){
        int per = scenario->getModeltime()->getyr_to_per( XMLHelper<int>::getAttr( curr, "year" ) );
        
        // Make sure that we had a valid year
        assert( per >= 0 );

        if( !mNationalAccounts[ per ] ) {
            mNationalAccounts[ per ] = new NationalAccount();
        }
        mNationalAccounts[ per ]->XMLParse( curr );
    }
    else {
        return false;
    }
    return true;
}

//! Output debug info for derived class
void RegionCGE::toDebugXMLDerived( const int period, std::ostream& out, Tabs* tabs ) const {
    mNationalAccounts[ period ]->toDebugXML( period, out, tabs );
    for ( unsigned int i = 0; i < finalDemandSector.size(); i ++ ) {
        finalDemandSector[i]->toDebugXML( period, out, tabs );
    }
    for( unsigned int i = 0; i < factorSupply.size(); i++ ){
        factorSupply[ i ]->toDebugXML( period, out, tabs );
    }
}

//! Complete the initialization.
void RegionCGE::completeInit() {
    Region::completeInit();

    // initialize demographic
    if( mDemographic ){
        mDemographic->completeInit();
    }
    for( SectorIterator sectorIter = mSupplySector.begin(); sectorIter != mSupplySector.end(); ++sectorIter ) {
        ( *sectorIter )->completeInit( 0, 0 );
    }
    for( unsigned int i = 0; i < finalDemandSector.size(); i++) {
        // Pass null for the dependency finder argument as CGE regions don't
        // have dependencies. Pass null for the regional land allocator 
        // because CGE regions do not currently use one.
        finalDemandSector[i]->completeInit( 0, 0 );
    }
    for( unsigned int i = 0; i < factorSupply.size(); i++) {
        factorSupply[i]->completeInit( mName );
    }
    // Make sure we have a NationalAccount for each period.
    for(unsigned int i = 0; i < mNationalAccounts.size(); i++) {
        if( !mNationalAccounts[i] ) {
            mNationalAccounts[i] = new NationalAccount();
        }
    }
    
    // initialize the calc capital good visitor
    mCalcCapitalGoodPriceVisitor = new CalcCapitalGoodPriceVisitor( mName );
}

//! Call any initializations that are only done once per period
void RegionCGE::initCalc( const int period ) 
{
    for (unsigned int i = 0; i < factorSupply.size(); i++) {
        factorSupply[i]->initCalc( mName, period );
    }

    for( SectorIterator currSector = mSupplySector.begin(); currSector != mSupplySector.end(); ++currSector ){
        (*currSector)->initCalc( mNationalAccounts[ period ], mDemographic, period );
    }

    // SGM sequence of procedures
    for ( unsigned int i = 0; i < finalDemandSector.size(); i++ ) {
        finalDemandSector[i]->initCalc( mNationalAccounts[ period ], mDemographic, period );
    }
    
    // TODO: should I move this to a Region::initCalc
    for( ResourceIterator currResource = mResources.begin(); currResource != mResources.end(); ++currResource ){
        (*currResource)->initCalc( mName, period );
    }
}

void RegionCGE::postCalc( const int aPeriod ){
    Region::postCalc( aPeriod );

    // do postCalc for consumers as well
    for ( unsigned int i = 0; i < finalDemandSector.size(); i++ ) {
        finalDemandSector[i]->postCalc( aPeriod );
    }

    // TODO: this is a total hack and would ideally be in the trade consumer
    // Account for exports and import.  We do this in postCalc because it is inconsequential to
    // operation however the national account is not passed down.  As a temporary solution we added it
    // into the market info for Capital and so now the region has to move it into the national accounts.
    Marketplace* marketplace = scenario->getMarketplace();
    IInfo* capitalMarketInfo = marketplace->getMarketInfo( "Capital", mName, aPeriod, true );

    // nominal is the current year quantity * the current year prices, real is the current year
    // quantity * base year prices
    double currExportsNominal = capitalMarketInfo->getDouble( "export-nominal", false );
    double currExportsReal = capitalMarketInfo->getDouble( "export-real", false );
    double currImportsNominal = capitalMarketInfo->getDouble( "import-nominal", false );
    double currImportsReal = capitalMarketInfo->getDouble( "import-real", false );

    NationalAccount* currAccounts = mNationalAccounts[ aPeriod ];
    currAccounts->addToAccount( NationalAccount::EXPORT_NOMINAL, currExportsNominal );
    currAccounts->addToAccount( NationalAccount::EXPORT_REAL, currExportsReal );
    currAccounts->addToAccount( NationalAccount::IMPORT_NOMINAL, currImportsNominal );
    currAccounts->addToAccount( NationalAccount::IMPORT_REAL, currImportsReal );

    // now take care of the stuff that should happen in the trade consumer
    currAccounts->setAccount( NationalAccount::NET_EXPORT_NOMINAL, currExportsNominal - currImportsNominal );
    currAccounts->setAccount( NationalAccount::NET_EXPORT_REAL, currExportsReal - currImportsReal );
    currAccounts->addToAccount( NationalAccount::GNP_NOMINAL, currExportsNominal - currImportsNominal );
    currAccounts->addToAccount( NationalAccount::GNP_REAL, currExportsReal - currImportsReal );
}

/*!
 * \brief Main regional calculation of economic supplies and demand
 * \details Calculates using the SGM order of operations
 * \param period The period to calculate.
 * \see RegionCGE::operate
 */
void RegionCGE::calc( const int period ) {
    // we must reset the national accounts so we do not have stale values from the
    // previous model calc.
    mNationalAccounts[ period ]->reset();

     /*!
      * \warning I am using doCalibrations to determine if this region is just being operated to calculate
      *          derivatives for a different region's good in which case this foreign region still needs to 
      *          operate the corresponding import sector since it would be affected by the price change.  See
      *          SolverLibrary::derivatives for more details.
      */
    IInfo* hack = scenario->getMarketplace()->getMarketInfo( "SVS", "USA", period, true );
    string hackStr = hack->getString( "CurrDerivRegion", false );
    bool doCalibrations = hackStr == mName || hackStr.empty();
    if( !doCalibrations ) {
        /*!
         * \todo Currently we are operating all import sectors  even though we should only be operating the import
         *       sector for the good that we are currently calculating the derivative for.  This was not done because
         *       the way SolverLibrary::derivatives is currently structured it does not have fine enough granularity to
         *       to know how much demand is given to the foreign good from any given sector before price perturbation.
         *       Profiling shows that there is significant perormance benefits to be had if we could get this to work.
         */
        for( vector<Sector*>::iterator currSec = mSupplySector.begin(); currSec != mSupplySector.end(); ++currSec ){
            if( (*currSec)->getName().rfind( "-import" ) != string::npos || (*currSec)->getName() == "TPT-Margin" ) {
                (*currSec)->operate( *mNationalAccounts[ period ], mDemographic, period );
            }
        }
        // the import taxes need to get added to the government taxes market but we don't really
        // need to run the consumers for that
        /*scenario->getMarketplace()->addToDemand( "government-taxes", name,
            mNationalAccounts[ period ]->getAccountValue( NationalAccount::INDIRECT_BUSINESS_TAX ), period );*/
        return;
    }
    // calls operate for both production and final demand sectors
    operate( period ); // This sector function operates existing capital, invests, and operates total.
    
    // calc resource supplies
    // TODO: should I just put this in operate?
    for( ResourceIterator currResource = mResources.begin(); currResource != mResources.end(); ++currResource ){
        (*currResource)->calcSupply( mName, 0, period );
    }
}

/*! 
 * \brief Function which operates the capital for all production sectors.
 * \details See the comments for RegionCGE for more details on the steps for CGE operation
 *          in SGM.
 * \param period The period to operate.
 */
void RegionCGE::operate( const int period ){
    Configuration* conf = Configuration::getInstance();
    // set the numeraire price manually to the price index
    // we should always do this since we can not rely on the 
    // numeraire region operating first
    Marketplace* marketplace = scenario->getMarketplace();
    string numeraireRegion = conf->getString( "numeraire-region", "USA" );
    string numeraireGood = conf->getString( "numeraire-good", "SVS" );
    double numerairePrice = marketplace->getPrice( "price-index", "USA", period );
    marketplace->setPrice( numeraireGood, numeraireRegion, numerairePrice, period, true );

    // calculate the price of the capital good before we operate production sectors
    // only need to visit the final demand sectors
    for (unsigned int i = 0; i < finalDemandSector.size(); i++) {
        finalDemandSector[i]->accept( mCalcCapitalGoodPriceVisitor, period );
    }
    
    // operate production sectors which will get old capital, distribute new investment, then operate
    // new capital.
    for( vector<Sector*>::iterator currSec = mSupplySector.begin(); currSec != mSupplySector.end(); ++currSec ){
        (*currSec)->operate( *mNationalAccounts[ period ], mDemographic, period );
    }

    // calculate demands from consumers which will also calculate supplies for factors not including resource
    for (unsigned int i = 0; i < finalDemandSector.size(); i++) {
        finalDemandSector[i]->operate( *mNationalAccounts[period], mDemographic, period );
    }
}

/*!
 * \brief Initialize the marketplaces in the base year to get initial demands for each region
 * \param period The period is usually the base period
 * \todo It is not clear if this method is still necessary.
 * \author Pralit Patel
 */
void RegionCGE::updateMarketplace( const int period ) {
    // have the proudction sectors and consumers add there initial demands into the marketplace
    for( vector<Sector*>::iterator currSector = mSupplySector.begin(); currSector != mSupplySector.end(); ++currSector ){
        (*currSector)->updateMarketplace( period );
    }
    for (unsigned int i = 0; i < finalDemandSector.size(); i++) {
        finalDemandSector[i]->updateMarketplace( period );
    }
}

void RegionCGE::accept( IVisitor* aVisitor, const int aPeriod ) const {
    aVisitor->startVisitRegionCGE( this, aPeriod );
    Region::accept( aVisitor, aPeriod );

    // For national account, if we have a -1 we will want to accept all of them
    // manually.
    if( aPeriod == -1 ){
        // there is one national account per period so we can use the index i
        // as the current period as well.
        for( unsigned int i = 0; i < mNationalAccounts.size(); ++i ){
            mNationalAccounts[ i ]->accept( aVisitor, i );
        }
    }
    else {
        mNationalAccounts[ aPeriod ]->accept( aVisitor, aPeriod );
    }

    // loop for final demand sectors
    for( CFinalDemandSectorIterator currSec = finalDemandSector.begin(); currSec != finalDemandSector.end(); ++currSec ){
        (*currSec)->accept( aVisitor, aPeriod );
    }
    // loop for factor supply sectors
    for( CFactorSupplyIterator currSec = factorSupply.begin(); currSec != factorSupply.end(); ++currSec ){
        (*currSec)->accept( aVisitor, aPeriod );
    }
    aVisitor->endVisitRegionCGE( this, aPeriod );
}

